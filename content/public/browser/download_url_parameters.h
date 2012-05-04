// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_BROWSER_DOWNLOAD_URL_PARAMETERS_H_
#define CONTENT_PUBLIC_BROWSER_DOWNLOAD_URL_PARAMETERS_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/callback.h"
#include "content/public/browser/download_id.h"
#include "content/public/browser/download_save_info.h"
#include "googleurl/src/gurl.h"
#include "net/base/net_errors.h"

namespace content {

class ResourceContext;
class ResourceDispatcherHost;
class WebContents;

// Pass an instance of DownloadUrlParameters to DownloadManager::DownloadUrl()
// to download the content at |url|. All parameters with setters are optional.
// |referrer| and |referrer_encoding| are the referrer for the download. If
// |prefer_cache| is true, then if the response to |url| is in the HTTP cache it
// will be used without revalidation. If |post_id| is non-negative, then it
// identifies the post transaction used to originally retrieve the |url|
// resource - it also requires |prefer_cache| to be |true| since re-post'ing is
// not done.  |save_info| specifies where the downloaded file should be saved,
// and whether the user should be prompted about the download.  If not null,
// |callback| will be called when the download starts, or if an error occurs
// that prevents a download item from being created.  We send a pointer to
// content::ResourceContext instead of the usual reference so that a copy of the
// object isn't made.

class CONTENT_EXPORT DownloadUrlParameters {
 public:
  // If there is an error, the DownloadId will be invalid.
  typedef base::Callback<void(DownloadId, net::Error)> OnStartedCallback;

  typedef std::pair<std::string, std::string> RequestHeadersNameValuePair;
  typedef std::vector<RequestHeadersNameValuePair> RequestHeadersType;

  static DownloadUrlParameters* FromWebContents(
      WebContents* web_contents,
      const GURL& url,
      const DownloadSaveInfo& save_info);

  DownloadUrlParameters(
      const GURL& url,
      int render_process_host_id,
      int render_view_host_routing_id,
      content::ResourceContext* resource_context,
      const DownloadSaveInfo& save_info);

  ~DownloadUrlParameters();

  void add_request_header(const std::string& name, const std::string& value) {
    request_headers_.push_back(make_pair(name, value));
  }
  void set_referrer(const GURL& referrer) { referrer_ = referrer; }
  void set_referrer_encoding(const std::string& referrer_encoding) {
    referrer_encoding_ = referrer_encoding;
  }
  void set_load_flags(int load_flags) { load_flags_ |= load_flags; }
  void set_method(const std::string& method) {
    method_ = method;
  }
  void set_post_body(const std::string& post_body) {
    post_body_ = post_body;
  }
  void set_prefer_cache(bool prefer_cache) {
    prefer_cache_ = prefer_cache;
  }
  void set_post_id(int64 post_id) { post_id_ = post_id; }
  void set_callback(const OnStartedCallback& callback) {
    callback_ = callback;
  }

  const OnStartedCallback& callback() const { return callback_; }
  int load_flags() const { return load_flags_; }
  const std::string& method() const { return method_; }
  const std::string& post_body() const { return post_body_; }
  int64 post_id() const { return post_id_; }
  bool prefer_cache() const { return prefer_cache_; }
  const GURL& referrer() const { return referrer_; }
  const std::string& referrer_encoding() const { return referrer_encoding_; }
  int render_process_host_id() const { return render_process_host_id_; }
  int render_view_host_routing_id() const {
    return render_view_host_routing_id_;
  }
  RequestHeadersType::const_iterator request_headers_begin() const {
    return request_headers_.begin();
  }
  RequestHeadersType::const_iterator request_headers_end() const {
    return request_headers_.end();
  }
  content::ResourceContext* resource_context() const {
    return resource_context_;
  }
  ResourceDispatcherHost* resource_dispatcher_host() const {
    return resource_dispatcher_host_;
  }
  const DownloadSaveInfo& save_info() const { return save_info_; }
  const GURL& url() const { return url_; }

 private:
  OnStartedCallback callback_;
  RequestHeadersType request_headers_;
  int load_flags_;
  std::string method_;
  std::string post_body_;
  int64 post_id_;
  bool prefer_cache_;
  GURL referrer_;
  std::string referrer_encoding_;
  int render_process_host_id_;
  int render_view_host_routing_id_;
  ResourceContext* resource_context_;
  ResourceDispatcherHost* resource_dispatcher_host_;
  DownloadSaveInfo save_info_;
  GURL url_;

  DISALLOW_COPY_AND_ASSIGN(DownloadUrlParameters);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_BROWSER_DOWNLOAD_URL_PARAMETERS_H_
