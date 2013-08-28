// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/url_request_info_util.h"

#include "base/logging.h"
#include "base/strings/string_util.h"
#include "content/common/fileapi/file_system_messages.h"
#include "content/renderer/pepper/common.h"
#include "content/renderer/pepper/host_globals.h"
#include "content/renderer/pepper/pepper_plugin_instance_impl.h"
#include "content/renderer/pepper/plugin_module.h"
#include "content/renderer/render_thread_impl.h"
#include "net/http/http_util.h"
#include "ppapi/proxy/ppapi_messages.h"
#include "ppapi/shared_impl/file_ref_detailed_info.h"
#include "ppapi/shared_impl/url_request_info_data.h"
#include "ppapi/shared_impl/var.h"
#include "ppapi/thunk/enter.h"
#include "third_party/WebKit/public/platform/WebData.h"
#include "third_party/WebKit/public/platform/WebHTTPBody.h"
#include "third_party/WebKit/public/platform/WebURL.h"
#include "third_party/WebKit/public/platform/WebURLRequest.h"
#include "third_party/WebKit/public/web/WebDocument.h"
#include "third_party/WebKit/public/web/WebFrame.h"
#include "url/gurl.h"
#include "url/url_util.h"
#include "webkit/child/weburlrequest_extradata_impl.h"

using ppapi::Resource;
using ppapi::URLRequestInfoData;
using ppapi::thunk::EnterResourceNoLock;
using WebKit::WebData;
using WebKit::WebHTTPBody;
using WebKit::WebString;
using WebKit::WebFrame;
using WebKit::WebURL;
using WebKit::WebURLRequest;

namespace content {

namespace {

// Appends the file ref given the Resource pointer associated with it to the
// given HTTP body, returning true on success.
bool AppendFileRefToBody(
    const ppapi::FileRefDetailedInfo& file_info,
    int64_t start_offset,
    int64_t number_of_bytes,
    PP_Time expected_last_modified_time,
    WebHTTPBody *http_body) {
  base::FilePath platform_path;
  switch (file_info.file_system_type) {
    case PP_FILESYSTEMTYPE_LOCALTEMPORARY:
    case PP_FILESYSTEMTYPE_LOCALPERSISTENT:
      // TODO(kinuko): remove this sync IPC when we fully support
      // AppendURLRange for FileSystem URL.
      RenderThreadImpl::current()->Send(
          new FileSystemHostMsg_SyncGetPlatformPath(
              GURL(file_info.file_system_url_spec), &platform_path));
      break;
    case PP_FILESYSTEMTYPE_EXTERNAL:
      platform_path = file_info.external_path;
      break;
    default:
      NOTREACHED();
  }
  http_body->appendFileRange(
      platform_path.AsUTF16Unsafe(),
      start_offset,
      number_of_bytes,
      expected_last_modified_time);
  return true;
}

// Checks that the request data is valid. Returns false on failure. Note that
// method and header validation is done by the URL loader when the request is
// opened, and any access errors are returned asynchronously.
bool ValidateURLRequestData(const URLRequestInfoData& data) {
  if (data.prefetch_buffer_lower_threshold < 0 ||
      data.prefetch_buffer_upper_threshold < 0 ||
      data.prefetch_buffer_upper_threshold <=
      data.prefetch_buffer_lower_threshold) {
    return false;
  }
  return true;
}

}  // namespace

bool CreateWebURLRequest(PP_Instance instance,
                         URLRequestInfoData* data,
                         WebFrame* frame,
                         WebURLRequest* dest) {
  // In the out-of-process case, we've received the URLRequestInfoData
  // from the untrusted plugin and done no validation on it. We need to be
  // sure it's not being malicious by checking everything for consistency.
  if (!ValidateURLRequestData(*data))
    return false;

  dest->initialize();
  dest->setTargetType(WebURLRequest::TargetIsObject);
  dest->setURL(frame->document().completeURL(WebString::fromUTF8(
      data->url)));
  dest->setDownloadToFile(data->stream_to_file);
  dest->setReportUploadProgress(data->record_upload_progress);

  if (!data->method.empty())
    dest->setHTTPMethod(WebString::fromUTF8(data->method));

  dest->setFirstPartyForCookies(frame->document().firstPartyForCookies());

  const std::string& headers = data->headers;
  if (!headers.empty()) {
    net::HttpUtil::HeadersIterator it(headers.begin(), headers.end(), "\n\r");
    while (it.GetNext()) {
      dest->addHTTPHeaderField(
          WebString::fromUTF8(it.name()),
          WebString::fromUTF8(it.values()));
    }
  }

  // Get file information for FileRefs inside BodyItems.
  std::vector<PP_Resource> resources;
  for (size_t i = 0; i < data->body.size(); ++i) {
    const URLRequestInfoData::BodyItem& item = data->body[i];
    if (item.is_file)
      resources.push_back(item.file_ref_pp_resource);
  }
  std::vector<ppapi::FileRefDetailedInfo> infos;

  if (!resources.empty()) {
    PepperPluginInstanceImpl* instance_impl =
        HostGlobals::Get()->GetInstance(instance);
    int child_process_id = instance_impl->module()->GetPluginChildId();
    // The routing id is automatically populated for us when set to 0.
    RenderThreadImpl::current()->Send(
        new PpapiHostMsg_FileRef_SyncGetInfoForRenderer(
            0, child_process_id, resources, &infos));
    if (infos.size() != resources.size())
      return false;
  }

  // Append the upload data.
  if (!data->body.empty()) {
    WebHTTPBody http_body;
    http_body.initialize();
    int file_index = 0;
    for (size_t i = 0; i < data->body.size(); ++i) {
      const URLRequestInfoData::BodyItem& item = data->body[i];
      if (item.is_file) {
        if (item.file_ref_pp_resource != infos[file_index].resource)
          return false;
        if (!AppendFileRefToBody(infos[file_index],
                                 item.start_offset,
                                 item.number_of_bytes,
                                 item.expected_last_modified_time,
                                 &http_body))
          return false;
        file_index++;
      } else {
        DCHECK(!item.data.empty());
        http_body.appendData(WebData(item.data));
      }
    }
    dest->setHTTPBody(http_body);
  }

  // Add the "Referer" header if there is a custom referrer. Such requests
  // require universal access. For all other requests, "Referer" will be set
  // after header security checks are done in AssociatedURLLoader.
  if (data->has_custom_referrer_url && !data->custom_referrer_url.empty())
    frame->setReferrerForRequest(*dest, GURL(data->custom_referrer_url));

  if (data->has_custom_content_transfer_encoding &&
      !data->custom_content_transfer_encoding.empty()) {
    dest->addHTTPHeaderField(
        WebString::fromUTF8("Content-Transfer-Encoding"),
        WebString::fromUTF8(data->custom_content_transfer_encoding));
  }

  if (data->has_custom_user_agent) {
    bool was_after_preconnect_request = false;
    dest->setExtraData(new webkit_glue::WebURLRequestExtraDataImpl(
        WebKit::WebReferrerPolicyDefault,  // Ignored.
        WebString::fromUTF8(data->custom_user_agent),
        was_after_preconnect_request));
  }

  return true;
}

bool URLRequestRequiresUniversalAccess(const URLRequestInfoData& data) {
  return
      data.has_custom_referrer_url ||
      data.has_custom_content_transfer_encoding ||
      data.has_custom_user_agent ||
      url_util::FindAndCompareScheme(data.url, "javascript", NULL);
}

}  // namespace content
