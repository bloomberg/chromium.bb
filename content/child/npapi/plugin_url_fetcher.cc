// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/child/npapi/plugin_url_fetcher.h"

#include "content/child/child_thread.h"
#include "content/child/npapi/webplugin.h"
#include "content/child/npapi/plugin_host.h"
#include "content/child/npapi/plugin_instance.h"
#include "content/child/npapi/plugin_stream_url.h"
#include "content/child/npapi/webplugin_resource_client.h"
#include "content/child/plugin_messages.h"
#include "content/child/resource_dispatcher.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "webkit/common/resource_request_body.h"

namespace content {

PluginURLFetcher::PluginURLFetcher(PluginStreamUrl* plugin_stream,
                                   const GURL& url,
                                   const GURL& first_party_for_cookies,
                                   const std::string& method,
                                   const std::string& post_data,
                                   const GURL& referrer,
                                   bool notify_redirects,
                                   bool is_plugin_src_load,
                                   int origin_pid,
                                   int render_view_id,
                                   unsigned long resource_id)
    : plugin_stream_(plugin_stream),
      url_(url),
      first_party_for_cookies_(first_party_for_cookies),
      method_(method),
      post_data_(post_data),
      referrer_(referrer),
      notify_redirects_(notify_redirects),
      is_plugin_src_load_(is_plugin_src_load),
      resource_id_(resource_id),
      data_offset_(0) {
  webkit_glue::ResourceLoaderBridge::RequestInfo request_info;
  request_info.method = method;
  request_info.url = url;
  request_info.first_party_for_cookies = first_party_for_cookies;
  request_info.referrer = referrer;
  request_info.load_flags = net::LOAD_NORMAL;
  request_info.requestor_pid = origin_pid;
  request_info.request_type = ResourceType::OBJECT;
  request_info.routing_id = render_view_id;

  std::vector<char> body;
  if (method == "POST") {
    std::vector<std::string> names;
    std::vector<std::string> values;
    PluginHost::SetPostData(
        post_data.c_str(), post_data.size(), &names, &values, &body);
    for (size_t i = 0; i < names.size(); ++i) {
      if (!request_info.headers.empty())
        request_info.headers += "\r\n";

      request_info.headers += names[i] + ": " + values[i];
    }
  }

  bridge_.reset(ChildThread::current()->resource_dispatcher()->CreateBridge(
      request_info));
  if (!body.empty()) {
    scoped_refptr<webkit_glue::ResourceRequestBody> request_body =
        new webkit_glue::ResourceRequestBody;
    request_body->AppendBytes(&body[0], body.size());
    bridge_->SetRequestBody(request_body.get());
  }

  bridge_->Start(this);

  // TODO(jam): range requests
}

PluginURLFetcher::~PluginURLFetcher() {
}

void PluginURLFetcher::Cancel() {
  bridge_->Cancel();
}

void PluginURLFetcher::URLRedirectResponse(bool allow) {
  if (allow) {
    bridge_->SetDefersLoading(true);
  } else {
    bridge_->Cancel();
    plugin_stream_->DidFail(resource_id_);  // That will delete |this|.
  }
}

void PluginURLFetcher::OnUploadProgress(uint64 position, uint64 size) {
}

bool PluginURLFetcher::OnReceivedRedirect(
    const GURL& new_url,
    const webkit_glue::ResourceResponseInfo& info,
    bool* has_new_first_party_for_cookies,
    GURL* new_first_party_for_cookies) {
  // TODO(jam): THIS LOGIC IS COPIED FROM WebPluginImpl::willSendRequest until
  // kDirectNPAPIRequests is the default and we can remove the old path there.

  // Currently this check is just to catch an https -> http redirect when
  // loading the main plugin src URL. Longer term, we could investigate
  // firing mixed diplay or scripting issues for subresource loads
  // initiated by plug-ins.
  if (is_plugin_src_load_ &&
      !plugin_stream_->instance()->webplugin()->CheckIfRunInsecureContent(
          new_url)) {
    plugin_stream_->DidFail(resource_id_);  // That will delete |this|.
    return false;
  }

  // It's unfortunate that this logic of when a redirect's method changes is
  // in url_request.cc, but weburlloader_impl.cc and this file have to duplicate
  // it instead of passing that information.
  if (info.headers->response_code() != 307)
    method_ = "GET";
  GURL old_url = url_;
  url_ = new_url;
  *has_new_first_party_for_cookies = true;
  *new_first_party_for_cookies = first_party_for_cookies_;

  // If the plugin does not participate in url redirect notifications then just
  // block cross origin 307 POST redirects.
  if (!notify_redirects_) {
    if (info.headers->response_code() == 307 && method_ == "POST" &&
        old_url.GetOrigin() != new_url.GetOrigin()) {
      plugin_stream_->DidFail(resource_id_);  // That will delete |this|.
      return false;
    }
  } else {
    // Pause the request while we ask the plugin what to do about the redirect.
    bridge_->SetDefersLoading(true);
    plugin_stream_->WillSendRequest(url_, info.headers->response_code());
  }

  return true;
}

void PluginURLFetcher::OnReceivedResponse(
    const webkit_glue::ResourceResponseInfo& info) {
  // TODO(jam): see WebPluginImpl::didReceiveResponse for request_is_seekable
  bool request_is_seekable = false;
  uint32 last_modified = 0;

  base::Time temp;
  if (info.headers->GetLastModifiedValue(&temp))
    last_modified = static_cast<uint32>(temp.ToDoubleT());

  std::string headers = info.headers->raw_headers();

  plugin_stream_->DidReceiveResponse(info.mime_type,
                              headers,
                              info.content_length,
                              last_modified,
                              request_is_seekable);
}

void PluginURLFetcher::OnDownloadedData(int len,
                                        int encoded_data_length) {
}

void PluginURLFetcher::OnReceivedData(const char* data,
                                      int data_length,
                                      int encoded_data_length) {
  plugin_stream_->DidReceiveData(data, data_length, data_offset_);
  data_offset_ += data_length;
}

void PluginURLFetcher::OnCompletedRequest(
    int error_code,
    bool was_ignored_by_handler,
    const std::string& security_info,
    const base::TimeTicks& completion_time) {
  if (error_code == net::OK) {
    plugin_stream_->DidFinishLoading(resource_id_);
  } else {
    plugin_stream_->DidFail(resource_id_);
  }
}

}  // namespace content
