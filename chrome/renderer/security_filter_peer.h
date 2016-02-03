// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SECURITY_FILTER_PEER_H_
#define CHROME_RENDERER_SECURITY_FILTER_PEER_H_

#include <stdint.h>

#include <string>

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "content/public/child/request_peer.h"
#include "content/public/common/resource_response_info.h"
#include "content/public/common/resource_type.h"

// The SecurityFilterPeer is a proxy to a
// content::RequestPeer instance.  It is used to pre-process
// unsafe resources (such as mixed-content resource).
// Call the factory method CreateSecurityFilterPeer() to obtain an instance of
// SecurityFilterPeer based on the original Peer.
class SecurityFilterPeer : public content::RequestPeer {
 public:
  ~SecurityFilterPeer() override;

  static scoped_ptr<content::RequestPeer>
  CreateSecurityFilterPeerForDeniedRequest(
      content::ResourceType resource_type,
      scoped_ptr<content::RequestPeer> peer,
      int os_error);

  static scoped_ptr<content::RequestPeer> CreateSecurityFilterPeerForFrame(
      scoped_ptr<content::RequestPeer> peer,
      int os_error);

  // content::RequestPeer methods.
  void OnUploadProgress(uint64_t position, uint64_t size) override;
  bool OnReceivedRedirect(const net::RedirectInfo& redirect_info,
                          const content::ResourceResponseInfo& info) override;
  void OnDownloadedData(int len, int encoded_data_length) override {}
 protected:
  explicit SecurityFilterPeer(scoped_ptr<content::RequestPeer> peer);

  scoped_ptr<content::RequestPeer> original_peer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SecurityFilterPeer);
};

// The BufferedPeer reads all the data of the request into an internal buffer.
// Subclasses should implement DataReady() to process the data as necessary.
class BufferedPeer : public SecurityFilterPeer {
 public:
  BufferedPeer(scoped_ptr<content::RequestPeer> peer,
               const std::string& mime_type);
  ~BufferedPeer() override;

  // content::RequestPeer Implementation.
  void OnReceivedResponse(const content::ResourceResponseInfo& info) override;
  void OnReceivedData(scoped_ptr<ReceivedData> data) override;
  void OnCompletedRequest(int error_code,
                          bool was_ignored_by_handler,
                          bool stale_copy_in_cache,
                          const std::string& security_info,
                          const base::TimeTicks& completion_time,
                          int64_t total_transfer_size) override;
  void OnReceivedCompletedResponse(const content::ResourceResponseInfo& info,
                                   scoped_ptr<ReceivedData> data,
                                   int error_code,
                                   bool was_ignored_by_handler,
                                   bool stale_copy_in_cache,
                                   const std::string& security_info,
                                   const base::TimeTicks& completion_time,
                                   int64_t total_transfer_size) override;

 protected:
  // Invoked when the entire request has been processed before the data is sent
  // to the original peer, giving an opportunity to subclasses to process the
  // data in data_.  If this method returns true, the data is fed to the
  // original peer, if it returns false, an error is sent instead.
  virtual bool DataReady() = 0;

  content::ResourceResponseInfo response_info_;
  std::string data_;

 private:
  std::string mime_type_;

  DISALLOW_COPY_AND_ASSIGN(BufferedPeer);
};

// The ReplaceContentPeer cancels the request and serves the provided data as
// content instead.
// TODO(jcampan): For now the resource is still being fetched, but ignored, as
// once we have provided the replacement content, the associated pending request
// in ResourceDispatcher is removed and further OnReceived* notifications are
// ignored.
class ReplaceContentPeer : public SecurityFilterPeer {
 public:
  ReplaceContentPeer(scoped_ptr<content::RequestPeer> peer,
                     const std::string& mime_type,
                     const std::string& data);
  ~ReplaceContentPeer() override;

  // content::RequestPeer Implementation.
  void OnReceivedResponse(const content::ResourceResponseInfo& info) override;
  void OnReceivedData(scoped_ptr<ReceivedData> data) override;
  void OnCompletedRequest(int error_code,
                          bool was_ignored_by_handler,
                          bool stale_copy_in_cache,
                          const std::string& security_info,
                          const base::TimeTicks& completion_time,
                          int64_t total_transfer_size) override;
  void OnReceivedCompletedResponse(const content::ResourceResponseInfo& info,
                                   scoped_ptr<ReceivedData> data,
                                   int error_code,
                                   bool was_ignored_by_handler,
                                   bool stale_copy_in_cache,
                                   const std::string& security_info,
                                   const base::TimeTicks& completion_time,
                                   int64_t total_transfer_size) override;

 private:
  content::ResourceResponseInfo response_info_;
  std::string mime_type_;
  std::string data_;

  DISALLOW_COPY_AND_ASSIGN(ReplaceContentPeer);
};

#endif  // CHROME_RENDERER_SECURITY_FILTER_PEER_H_
