// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_MESSAGE_FILTER_PEER_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_MESSAGE_FILTER_PEER_H_

#include <string>

#include "chrome/common/filter_policy.h"
#include "ipc/ipc_message.h"
#include "webkit/glue/resource_loader_bridge.h"

// The ExtensionMessageFilterPeer is a proxy to a
// webkit_glue::ResourceLoaderBridge::Peer instance.  It is used to pre-process
// extension resources (such as css files).
// Call the factory method CreateExtensionMessageFilterPeer() to obtain an
// instance of ExtensionMessageFilterPeer based on the original Peer.
class ExtensionMessageFilterPeer
    : public webkit_glue::ResourceLoaderBridge::Peer {
 public:
  virtual ~ExtensionMessageFilterPeer();

  static ExtensionMessageFilterPeer* CreateExtensionMessageFilterPeer(
      webkit_glue::ResourceLoaderBridge::Peer* peer,
      IPC::Message::Sender* message_sender,
      const std::string& mime_type,
      FilterPolicy::Type filter_policy,
      const GURL& request_url);

  // ResourceLoaderBridge::Peer methods.
  virtual void OnUploadProgress(uint64 position, uint64 size);
  virtual bool OnReceivedRedirect(
      const GURL& new_url,
      const webkit_glue::ResourceLoaderBridge::ResponseInfo& info,
      bool* has_new_first_party_for_cookies,
      GURL* new_first_party_for_cookies);
  virtual void OnReceivedResponse(
      const webkit_glue::ResourceLoaderBridge::ResponseInfo& info,
      bool content_filtered);
  virtual void OnReceivedData(const char* data, int len);
  virtual void OnCompletedRequest(const URLRequestStatus& status,
                                  const std::string& security_info);
  virtual GURL GetURLForDebugging() const;

 private:
  friend class ExtensionMessageFilterPeerTest;

  // Use CreateExtensionMessageFilterPeer to create an instance.
  ExtensionMessageFilterPeer(
      webkit_glue::ResourceLoaderBridge::Peer* peer,
      IPC::Message::Sender* message_sender,
      const GURL& request_url);

  // Loads message catalogs, and replaces all __MSG_some_name__ templates within
  // loaded file.
  void ReplaceMessages();

  // Original peer that handles the request once we are done processing data_.
  webkit_glue::ResourceLoaderBridge::Peer* original_peer_;

  // We just pass though the response info. This holds the copy of the original.
  webkit_glue::ResourceLoaderBridge::ResponseInfo response_info_;

  // Sends ViewHostMsg_GetExtensionMessageBundle message to the browser to fetch
  // message catalog.
  IPC::Message::Sender* message_sender_;

  // Buffer for incoming data. We wait until OnCompletedRequest before using it.
  std::string data_;

  // Original request URL.
  GURL request_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageFilterPeer);
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_MESSAGE_FILTER_PEER_H_
