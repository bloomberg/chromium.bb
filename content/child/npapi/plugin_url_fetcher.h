// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_NPAPI_PLUGIN_URL_FETCHER_H_
#define CONTENT_CHILD_NPAPI_PLUGIN_URL_FETCHER_H_

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "content/public/child/request_peer.h"
#include "content/public/common/referrer.h"
#include "url/gurl.h"

namespace content {
class MultipartResponseDelegate;
class PluginStreamUrl;

// Fetches URLS for a plugin using ResourceDispatcher.
class PluginURLFetcher : public RequestPeer {
 public:
  PluginURLFetcher(PluginStreamUrl* plugin_stream,
                   const GURL& url,
                   const GURL& first_party_for_cookies,
                   const std::string& method,
                   const char* buf,
                   unsigned int len,
                   const Referrer& referrer,
                   const std::string& range,
                   bool notify_redirects,
                   bool is_plugin_src_load,
                   int origin_pid,
                   int render_frame_id,
                   int render_view_id,
                   unsigned long resource_id,
                   bool copy_stream_data);
  ~PluginURLFetcher() override;

  // Cancels the current request.
  void Cancel();

  // Called with the plugin's reply to NPP_URLRedirectNotify.
  void URLRedirectResponse(bool allow);

  GURL first_party_for_cookies() { return first_party_for_cookies_; }
  Referrer referrer() { return referrer_; }
  int origin_pid() { return origin_pid_; }
  int render_frame_id() { return render_frame_id_; }
  int render_view_id() { return render_view_id_; }
  bool copy_stream_data() { return copy_stream_data_; }
  bool pending_failure_notification() { return pending_failure_notification_; }

 private:
  // RequestPeer implementation:
  void OnUploadProgress(uint64 position, uint64 size) override;
  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          const ResourceResponseInfo& info) override;
  void OnReceivedResponse(const ResourceResponseInfo& info) override;
  void OnDownloadedData(int len, int encoded_data_length) override;
  void OnReceivedData(scoped_ptr<ReceivedData> data) override;
  void OnCompletedRequest(int error_code,
                          bool was_ignored_by_handler,
                          bool stale_copy_in_cache,
                          const std::string& security_info,
                          const base::TimeTicks& completion_time,
                          int64 total_transfer_size) override;
  void OnReceivedCompletedResponse(const ResourceResponseInfo& info,
                                   scoped_ptr<ReceivedData> data,
                                   int error_code,
                                   bool was_ignored_by_handler,
                                   bool stale_copy_in_cache,
                                   const std::string& security_info,
                                   const base::TimeTicks& completion_time,
                                   int64 total_transfer_size) override;

  // |plugin_stream_| becomes NULL after Cancel() to ensure no further calls
  // |reach it.
  PluginStreamUrl* plugin_stream_;
  GURL url_;
  GURL first_party_for_cookies_;
  Referrer referrer_;
  bool notify_redirects_;
  bool is_plugin_src_load_;
  int origin_pid_;
  int render_frame_id_;
  int render_view_id_;
  unsigned long resource_id_;
  bool copy_stream_data_;
  int64 data_offset_;
  bool pending_failure_notification_;
  int request_id_;

  scoped_ptr<MultipartResponseDelegate> multipart_delegate_;
  base::WeakPtrFactory<PluginURLFetcher> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(PluginURLFetcher);
};

}  // namespace content

#endif  // CONTENT_CHILD_NPAPI_PLUGIN_URL_FETCHER_H_
