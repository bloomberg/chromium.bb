// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SECURITY_FILTER_PEER_H_
#define CHROME_RENDERER_SECURITY_FILTER_PEER_H_

#include "content/public/child/request_peer.h"
#include "content/public/common/resource_response_info.h"
#include "content/public/common/resource_type.h"

// The SecurityFilterPeer is a proxy to a
// content::RequestPeer instance.  It is used to pre-process
// unsafe resources (such as mixed-content resource).
// Call the factory method CreateSecurityFilterPeer() to obtain an instance of
// SecurityFilterPeer based on the original Peer.
// NOTE: subclasses should ensure they delete themselves at the end of the
// OnReceiveComplete call.
class SecurityFilterPeer : public content::RequestPeer {
 public:
  virtual ~SecurityFilterPeer();

  static SecurityFilterPeer* CreateSecurityFilterPeerForDeniedRequest(
      content::ResourceType resource_type,
      content::RequestPeer* peer,
      int os_error);

  static SecurityFilterPeer* CreateSecurityFilterPeerForFrame(
      content::RequestPeer* peer,
      int os_error);

  // content::RequestPeer methods.
  virtual void OnUploadProgress(uint64 position, uint64 size) OVERRIDE;
  virtual bool OnReceivedRedirect(
      const net::RedirectInfo& redirect_info,
      const content::ResourceResponseInfo& info) OVERRIDE;
  virtual void OnReceivedResponse(
      const content::ResourceResponseInfo& info) OVERRIDE;
  virtual void OnDownloadedData(int len, int encoded_data_length) OVERRIDE {}
  virtual void OnReceivedData(const char* data,
                              int data_length,
                              int encoded_data_length) OVERRIDE;
  virtual void OnCompletedRequest(int error_code,
                                  bool was_ignored_by_handler,
                                  bool stale_copy_in_cache,
                                  const std::string& security_info,
                                  const base::TimeTicks& completion_time,
                                  int64 total_transfer_size) OVERRIDE;

 protected:
  explicit SecurityFilterPeer(content::RequestPeer* peer);

  content::RequestPeer* original_peer_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SecurityFilterPeer);
};

// The BufferedPeer reads all the data of the request into an internal buffer.
// Subclasses should implement DataReady() to process the data as necessary.
class BufferedPeer : public SecurityFilterPeer {
 public:
  BufferedPeer(content::RequestPeer* peer, const std::string& mime_type);
  virtual ~BufferedPeer();

  // content::RequestPeer Implementation.
  virtual void OnReceivedResponse(
      const content::ResourceResponseInfo& info) OVERRIDE;
  virtual void OnReceivedData(const char* data,
                              int data_length,
                              int encoded_data_length) OVERRIDE;
  virtual void OnCompletedRequest(
      int error_code,
      bool was_ignored_by_handler,
      bool stale_copy_in_cache,
      const std::string& security_info,
      const base::TimeTicks& completion_time,
      int64 total_transfer_size) OVERRIDE;

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
  ReplaceContentPeer(content::RequestPeer* peer,
                     const std::string& mime_type,
                     const std::string& data);
  virtual ~ReplaceContentPeer();

  // content::RequestPeer Implementation.
  virtual void OnReceivedResponse(
      const content::ResourceResponseInfo& info) OVERRIDE;
  virtual void OnReceivedData(const char* data,
                              int data_length,
                              int encoded_data_length) OVERRIDE;
  virtual void OnCompletedRequest(
      int error_code,
      bool was_ignored_by_handler,
      bool stale_copy_in_cache,
      const std::string& security_info,
      const base::TimeTicks& completion_time,
      int64 total_transfer_size) OVERRIDE;

 private:
  content::ResourceResponseInfo response_info_;
  std::string mime_type_;
  std::string data_;

  DISALLOW_COPY_AND_ASSIGN(ReplaceContentPeer);
};

#endif  // CHROME_RENDERER_SECURITY_FILTER_PEER_H_
