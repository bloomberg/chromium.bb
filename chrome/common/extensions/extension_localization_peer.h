// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_EXTENSIONS_EXTENSION_LOCALIZATION_PEER_H_
#define CHROME_COMMON_EXTENSIONS_EXTENSION_LOCALIZATION_PEER_H_
#pragma once

#include <string>

#include "ipc/ipc_message.h"
#include "webkit/glue/resource_loader_bridge.h"

// The ExtensionLocalizationPeer is a proxy to a
// webkit_glue::ResourceLoaderBridge::Peer instance.  It is used to pre-process
// CSS files requested by extensions to replace localization templates with the
// appropriate localized strings.
//
// Call the factory method CreateExtensionLocalizationPeer() to obtain an
// instance of ExtensionLocalizationPeer based on the original Peer.
class ExtensionLocalizationPeer
    : public webkit_glue::ResourceLoaderBridge::Peer {
 public:
  virtual ~ExtensionLocalizationPeer();

  static ExtensionLocalizationPeer* CreateExtensionLocalizationPeer(
      webkit_glue::ResourceLoaderBridge::Peer* peer,
      IPC::Message::Sender* message_sender,
      const std::string& mime_type,
      const GURL& request_url);

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
  virtual void OnReceivedData(const char* data,
                              int data_length,
                              int encoded_data_length);
  virtual void OnCompletedRequest(const net::URLRequestStatus& status,
                                  const std::string& security_info,
                                  const base::Time& completion_time);

 private:
  friend class ExtensionLocalizationPeerTest;

  // Use CreateExtensionLocalizationPeer to create an instance.
  ExtensionLocalizationPeer(
      webkit_glue::ResourceLoaderBridge::Peer* peer,
      IPC::Message::Sender* message_sender,
      const GURL& request_url);

  // Loads message catalogs, and replaces all __MSG_some_name__ templates within
  // loaded file.
  void ReplaceMessages();

  // Original peer that handles the request once we are done processing data_.
  webkit_glue::ResourceLoaderBridge::Peer* original_peer_;

  // We just pass though the response info. This holds the copy of the original.
  webkit_glue::ResourceResponseInfo response_info_;

  // Sends ExtensionHostMsg_GetMessageBundle message to the browser to fetch
  // message catalog.
  IPC::Message::Sender* message_sender_;

  // Buffer for incoming data. We wait until OnCompletedRequest before using it.
  std::string data_;

  // Original request URL.
  GURL request_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionLocalizationPeer);
};

#endif  // CHROME_COMMON_EXTENSIONS_EXTENSION_LOCALIZATION_PEER_H_
