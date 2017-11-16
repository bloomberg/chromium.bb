// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/browser/api/web_request/web_request_event_details.h"

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string_number_conversions.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/resource_request_info.h"
#include "content/public/browser/websocket_handshake_request_info.h"
#include "content/public/common/child_process_host.h"
#include "extensions/browser/api/extensions_api_client.h"
#include "extensions/browser/api/web_request/upload_data_presenter.h"
#include "extensions/browser/api/web_request/web_request_api_constants.h"
#include "extensions/browser/api/web_request/web_request_api_helpers.h"
#include "extensions/browser/api/web_request/web_request_permissions.h"
#include "extensions/browser/api/web_request/web_request_resource_type.h"
#include "extensions/common/permissions/permissions_data.h"
#include "ipc/ipc_message.h"
#include "net/base/auth.h"
#include "net/base/upload_data_stream.h"
#include "net/http/http_request_headers.h"
#include "net/http/http_response_headers.h"
#include "net/url_request/url_request.h"

using extension_web_request_api_helpers::ExtraInfoSpec;

namespace helpers = extension_web_request_api_helpers;
namespace keys = extension_web_request_api_constants;

namespace extensions {

WebRequestEventDetails::WebRequestEventDetails(const net::URLRequest* request,
                                               int extra_info_spec)
    : extra_info_spec_(extra_info_spec),
      render_process_id_(content::ChildProcessHost::kInvalidUniqueID),
      render_frame_id_(MSG_ROUTING_NONE) {
  auto resource_type = GetWebRequestResourceType(request);
  const content::ResourceRequestInfo* info =
      content::ResourceRequestInfo::ForRequest(request);
  if (info) {
    render_process_id_ = info->GetChildID();
    render_frame_id_ = info->GetRenderFrameID();
  } else if (resource_type == WebRequestResourceType::WEB_SOCKET) {
    // TODO(pkalinnikov): Consider embedding WebSocketHandshakeRequestInfo into
    // UrlRequestUserData.
    const content::WebSocketHandshakeRequestInfo* ws_info =
        content::WebSocketHandshakeRequestInfo::ForRequest(request);
    if (ws_info) {
      render_process_id_ = ws_info->GetChildId();
      render_frame_id_ = ws_info->GetRenderFrameId();
    }
  } else {
    // Fallback for requests that are not allocated by a
    // ResourceDispatcherHost, such as the TemplateURLFetcher.
    content::ResourceRequestInfo::GetRenderFrameForRequest(
        request, &render_process_id_, &render_frame_id_);
  }

  dict_.SetString(keys::kMethodKey, request->method());
  dict_.SetString(keys::kRequestIdKey,
                  base::Uint64ToString(request->identifier()));
  dict_.SetDouble(keys::kTimeStampKey, base::Time::Now().ToDoubleT() * 1000);
  dict_.SetString(keys::kTypeKey,
                  WebRequestResourceTypeToString(resource_type));
  dict_.SetString(keys::kUrlKey, request->url().spec());
  if (request->initiator())
    initiator_ = request->initiator();
}

WebRequestEventDetails::~WebRequestEventDetails() {}

void WebRequestEventDetails::SetRequestBody(const net::URLRequest* request) {
  if (!(extra_info_spec_ & ExtraInfoSpec::REQUEST_BODY))
    return;

  const net::UploadDataStream* upload_data = request->get_upload();
  if (!upload_data ||
      (request->method() != "POST" && request->method() != "PUT"))
    return;

  base::DictionaryValue* request_body = new base::DictionaryValue();
  request_body_.reset(request_body);

  // Get the data presenters, ordered by how specific they are.
  ParsedDataPresenter parsed_data_presenter(*request);
  RawDataPresenter raw_data_presenter;
  UploadDataPresenter* const presenters[] = {
      &parsed_data_presenter,  // 1: any parseable forms? (Specific to forms.)
      &raw_data_presenter      // 2: any data at all? (Non-specific.)
  };
  // Keys for the results of the corresponding presenters.
  static const char* const kKeys[] = {keys::kRequestBodyFormDataKey,
                                      keys::kRequestBodyRawKey};

  const std::vector<std::unique_ptr<net::UploadElementReader>>* readers =
      upload_data->GetElementReaders();
  bool some_succeeded = false;
  if (readers) {
    for (size_t i = 0; i < arraysize(presenters); ++i) {
      for (const auto& reader : *readers)
        presenters[i]->FeedNext(*reader);
      if (presenters[i]->Succeeded()) {
        request_body->Set(kKeys[i], presenters[i]->Result());
        some_succeeded = true;
        break;
      }
    }
  }
  if (!some_succeeded)
    request_body->SetString(keys::kRequestBodyErrorKey, "Unknown error.");
}

void WebRequestEventDetails::SetRequestHeaders(
    const net::HttpRequestHeaders& request_headers) {
  if (!(extra_info_spec_ & ExtraInfoSpec::REQUEST_HEADERS))
    return;

  base::ListValue* headers = new base::ListValue();
  for (net::HttpRequestHeaders::Iterator it(request_headers); it.GetNext();)
    headers->Append(helpers::CreateHeaderDictionary(it.name(), it.value()));
  request_headers_.reset(headers);
}

void WebRequestEventDetails::SetAuthInfo(
    const net::AuthChallengeInfo& auth_info) {
  dict_.SetBoolean(keys::kIsProxyKey, auth_info.is_proxy);
  if (!auth_info.scheme.empty())
    dict_.SetString(keys::kSchemeKey, auth_info.scheme);
  if (!auth_info.realm.empty())
    dict_.SetString(keys::kRealmKey, auth_info.realm);
  auto challenger = std::make_unique<base::DictionaryValue>();
  challenger->SetString(keys::kHostKey, auth_info.challenger.host());
  challenger->SetInteger(keys::kPortKey, auth_info.challenger.port());
  dict_.Set(keys::kChallengerKey, std::move(challenger));
}

void WebRequestEventDetails::SetResponseHeaders(
    const net::URLRequest* request,
    const net::HttpResponseHeaders* response_headers) {
  if (!response_headers) {
    // Not all URLRequestJobs specify response headers. E.g. URLRequestFTPJob,
    // URLRequestFileJob and some redirects.
    dict_.SetInteger(keys::kStatusCodeKey, request->GetResponseCode());
    dict_.SetString(keys::kStatusLineKey, "");
  } else {
    dict_.SetInteger(keys::kStatusCodeKey, response_headers->response_code());
    dict_.SetString(keys::kStatusLineKey, response_headers->GetStatusLine());
  }

  if (extra_info_spec_ & ExtraInfoSpec::RESPONSE_HEADERS) {
    base::ListValue* headers = new base::ListValue();
    if (response_headers) {
      size_t iter = 0;
      std::string name;
      std::string value;
      while (response_headers->EnumerateHeaderLines(&iter, &name, &value)) {
        if (ExtensionsAPIClient::Get()->ShouldHideResponseHeader(request->url(),
                                                                 name)) {
          continue;
        }
        headers->Append(helpers::CreateHeaderDictionary(name, value));
      }
    }
    response_headers_.reset(headers);
  }
}

void WebRequestEventDetails::SetResponseSource(const net::URLRequest* request) {
  dict_.SetBoolean(keys::kFromCache, request->was_cached());
  const std::string response_ip = request->GetSocketAddress().host();
  if (!response_ip.empty())
    dict_.SetString(keys::kIpKey, response_ip);
}

void WebRequestEventDetails::SetFrameData(
    const ExtensionApiFrameIdMap::FrameData& frame_data) {
  dict_.SetInteger(keys::kTabIdKey, frame_data.tab_id);
  dict_.SetInteger(keys::kFrameIdKey, frame_data.frame_id);
  dict_.SetInteger(keys::kParentFrameIdKey, frame_data.parent_frame_id);
}

void WebRequestEventDetails::DetermineFrameDataOnUI() {
  DCHECK_CURRENTLY_ON(content::BrowserThread::UI);
  content::RenderFrameHost* rfh =
      content::RenderFrameHost::FromID(render_process_id_, render_frame_id_);
  ExtensionApiFrameIdMap::FrameData frame_data =
      ExtensionApiFrameIdMap::Get()->GetFrameData(rfh);
  SetFrameData(frame_data);
}

void WebRequestEventDetails::DetermineFrameDataOnIO(
    const DeterminedFrameDataCallback& callback) {
  std::unique_ptr<WebRequestEventDetails> self(this);
  ExtensionApiFrameIdMap::Get()->GetFrameDataOnIO(
      render_process_id_, render_frame_id_,
      base::Bind(&WebRequestEventDetails::OnDeterminedFrameData,
                 base::Unretained(this), base::Passed(&self), callback));
}

std::unique_ptr<base::DictionaryValue> WebRequestEventDetails::GetFilteredDict(
    int extra_info_spec,
    const extensions::InfoMap* extension_info_map,
    const extensions::ExtensionId& extension_id,
    bool crosses_incognito) const {
  std::unique_ptr<base::DictionaryValue> result = dict_.CreateDeepCopy();
  if ((extra_info_spec & ExtraInfoSpec::REQUEST_BODY) && request_body_) {
    result->SetKey(keys::kRequestBodyKey, request_body_->Clone());
  }
  if ((extra_info_spec & ExtraInfoSpec::REQUEST_HEADERS) && request_headers_) {
    result->SetKey(keys::kRequestHeadersKey, request_headers_->Clone());
  }
  if ((extra_info_spec & ExtraInfoSpec::RESPONSE_HEADERS) &&
      response_headers_) {
    result->SetKey(keys::kResponseHeadersKey, response_headers_->Clone());
  }

  // Only listeners with a permission for the initiator should recieve it.
  if (extension_info_map && initiator_) {
    int tab_id = -1;
    dict_.GetInteger(keys::kTabIdKey, &tab_id);
    if (WebRequestPermissions::CanExtensionAccessInitiator(
            extension_info_map, extension_id, initiator_, tab_id,
            crosses_incognito)) {
      result->SetString(keys::kInitiatorKey, initiator_->Serialize());
    }
  }
  return result;
}

std::unique_ptr<base::DictionaryValue>
WebRequestEventDetails::GetAndClearDict() {
  std::unique_ptr<base::DictionaryValue> result(new base::DictionaryValue);
  dict_.Swap(result.get());
  return result;
}

std::unique_ptr<WebRequestEventDetails>
WebRequestEventDetails::CreatePublicSessionCopy() {
  std::unique_ptr<WebRequestEventDetails> copy(new WebRequestEventDetails);
  copy->initiator_ = initiator_;
  copy->render_process_id_ = render_process_id_;
  copy->render_frame_id_ = render_frame_id_;

  static const char* const kSafeAttributes[] = {
    "method", "requestId", "timeStamp", "type", "tabId", "frameId",
    "parentFrameId", "fromCache", "error", "ip", "statusLine", "statusCode"
  };

  for (const char* safe_attr : kSafeAttributes) {
    base::Value* val = dict_.FindKey(safe_attr);
    if (val)
      copy->dict_.SetKey(safe_attr, val->Clone());
  }

  // URL is stripped down to the origin.
  std::string url;
  dict_.GetString(keys::kUrlKey, &url);
  GURL gurl(url);
  copy->dict_.SetString(keys::kUrlKey, gurl.GetOrigin().spec());

  return copy;
}

WebRequestEventDetails::WebRequestEventDetails()
    : extra_info_spec_(0), render_process_id_(0), render_frame_id_(0) {}

void WebRequestEventDetails::OnDeterminedFrameData(
    std::unique_ptr<WebRequestEventDetails> self,
    const DeterminedFrameDataCallback& callback,
    const ExtensionApiFrameIdMap::FrameData& frame_data) {
  SetFrameData(frame_data);
  callback.Run(std::move(self));
}

}  // namespace extensions
