// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_LOADER_TEST_REQUEST_PEER_H_
#define CONTENT_RENDERER_LOADER_TEST_REQUEST_PEER_H_

#include <stdint.h>
#include <memory>
#include <string>
#include <vector>
#include "base/time/time.h"
#include "content/public/renderer/request_peer.h"

namespace net {
struct RedirectInfo;
}  // namespace net

namespace content {

class ReceivedData;
class ResourceDispatcher;
struct ResourceResponseInfo;

// Listens for request response data and stores it so that it can be compared
// to the reference data.
class TestRequestPeer : public RequestPeer {
 public:
  struct Context;
  TestRequestPeer(ResourceDispatcher* dispatcher, Context* context);
  ~TestRequestPeer() override;

  void OnUploadProgress(uint64_t position, uint64_t size) override;
  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          const ResourceResponseInfo& info) override;
  void OnReceivedResponse(const ResourceResponseInfo& info) override;
  void OnDownloadedData(int len, int encoded_data_length) override;
  void OnReceivedData(std::unique_ptr<ReceivedData> data) override;
  void OnTransferSizeUpdated(int transfer_size_diff) override;
  void OnReceivedCachedMetadata(const char* data, int len) override;
  void OnCompletedRequest(int error_code,
                          bool stale_copy_in_cache,
                          const base::TimeTicks& completion_time,
                          int64_t total_transfer_size,
                          int64_t encoded_body_size,
                          int64_t decoded_body_size) override;

  struct Context final {
    Context();
    ~Context();

    // True if should follow redirects, false if should cancel them.
    bool follow_redirects = true;
    // True if the request should be deferred on redirects.
    bool defer_on_redirect = false;

    // Number of total redirects seen.
    int seen_redirects = 0;

    bool cancel_on_receive_response = false;
    bool cancel_on_receive_data = false;
    bool received_response = false;

    std::vector<char> cached_metadata;
    // Data received. If downloading to file, remains empty.
    std::string data;

    // Total encoded data length, regardless of whether downloading to a file or
    // not.
    int total_encoded_data_length = 0;
    bool defer_on_transfer_size_updated = false;

    // Total length when downloading to a file.
    int total_downloaded_data_length = 0;

    bool complete = false;
    bool cancelled = false;
    int request_id = -1;
  };

 private:
  ResourceDispatcher* dispatcher_;
  Context* context_;

  DISALLOW_COPY_AND_ASSIGN(TestRequestPeer);
};

}  // namespace content

#endif  // CONTENT_RENDERER_LOADER_TEST_REQUEST_PEER_H_
