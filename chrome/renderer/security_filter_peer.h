// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_SECURITY_FILTER_PEER_H_
#define CHROME_RENDERER_SECURITY_FILTER_PEER_H_
#pragma once

#include "webkit/glue/resource_loader_bridge.h"

// The SecurityFilterPeer is a proxy to a
// webkit_glue::ResourceLoaderBridge::Peer instance.  It is used to pre-process
// unsafe resources (such as mixed-content resource).
// Call the factory method CreateSecurityFilterPeer() to obtain an instance of
// SecurityFilterPeer based on the original Peer.
// NOTE: subclasses should insure they delete themselves at the end of the
// OnReceiveComplete call.
class SecurityFilterPeer : public webkit_glue::ResourceLoaderBridge::Peer {
 public:
  virtual ~SecurityFilterPeer();

  static SecurityFilterPeer* CreateSecurityFilterPeerForDeniedRequest(
      ResourceType::Type resource_type,
      webkit_glue::ResourceLoaderBridge::Peer* peer,
      int os_error);

  static SecurityFilterPeer* CreateSecurityFilterPeerForFrame(
      webkit_glue::ResourceLoaderBridge::Peer* peer,
      int os_error);

  // ResourceLoaderBridge::Peer methods.
  virtual void OnUploadProgress(uint64 position, uint64 size);
  virtual bool OnReceivedRedirect(
      const GURL& new_url,
      const webkit_glue::ResourceResponseInfo& info,
      bool* has_new_first_party_for_cookies,
      GURL* new_first_party_for_cookies);
  virtual void OnReceivedResponse(
      const webkit_glue::ResourceResponseInfo& info);
  virtual void OnDownloadedData(int len) {}
  virtual void OnReceivedData(const char* data, int len);
  virtual void OnCompletedRequest(const net::URLRequestStatus& status,
                                  const std::string& security_info,
                                  const base::Time& completion_time);

 protected:
  SecurityFilterPeer(webkit_glue::ResourceLoaderBridge* resource_loader_bridge,
                     webkit_glue::ResourceLoaderBridge::Peer* peer);

  webkit_glue::ResourceLoaderBridge::Peer* original_peer_;
  webkit_glue::ResourceLoaderBridge* resource_loader_bridge_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SecurityFilterPeer);
};

// The BufferedPeer reads all the data of the request into an internal buffer.
// Subclasses should implement DataReady() to process the data as necessary.
class BufferedPeer : public SecurityFilterPeer {
 public:
  BufferedPeer(webkit_glue::ResourceLoaderBridge* resource_loader_bridge,
               webkit_glue::ResourceLoaderBridge::Peer* peer,
               const std::string& mime_type);
  virtual ~BufferedPeer();

  // ResourceLoaderBridge::Peer Implementation.
  virtual void OnReceivedResponse(
      const webkit_glue::ResourceResponseInfo& info);
  virtual void OnReceivedData(const char* data, int len);
  virtual void OnCompletedRequest(const net::URLRequestStatus& status,
                                  const std::string& security_info,
                                  const base::Time& completion_time);

 protected:
  // Invoked when the entire request has been processed before the data is sent
  // to the original peer, giving an opportunity to subclasses to process the
  // data in data_.  If this method returns true, the data is fed to the
  // original peer, if it returns false, an error is sent instead.
  virtual bool DataReady() = 0;

  webkit_glue::ResourceResponseInfo response_info_;
  std::string data_;

 private:
  std::string mime_type_;

  DISALLOW_COPY_AND_ASSIGN(BufferedPeer);
};

// The ReplaceContentPeer cancels the request and serves the provided data as
// content instead.
// TODO(jcampan): we do not as of now cancel the request, as we do not have
// access to the resource_loader_bridge in the SecurityFilterPeer factory
// method.  For now the resource is still being fetched, but ignored, as once
// we have provided the replacement content, the associated pending request
// in ResourceDispatcher is removed and further OnReceived* notifications are
// ignored.
class ReplaceContentPeer : public SecurityFilterPeer {
 public:
  ReplaceContentPeer(webkit_glue::ResourceLoaderBridge* resource_loader_bridge,
                     webkit_glue::ResourceLoaderBridge::Peer* peer,
                     const std::string& mime_type,
                     const std::string& data);
  virtual ~ReplaceContentPeer();

  // ResourceLoaderBridge::Peer Implementation.
  virtual void OnReceivedResponse(
      const webkit_glue::ResourceResponseInfo& info);
  virtual void OnReceivedData(const char* data, int len);
  virtual void OnCompletedRequest(const net::URLRequestStatus& status,
                                  const std::string& security_info,
                                  const base::Time& completion_time);

 private:
  webkit_glue::ResourceResponseInfo response_info_;
  std::string mime_type_;
  std::string data_;

  DISALLOW_COPY_AND_ASSIGN(ReplaceContentPeer);
};

#endif  // CHROME_RENDERER_SECURITY_FILTER_PEER_H_
