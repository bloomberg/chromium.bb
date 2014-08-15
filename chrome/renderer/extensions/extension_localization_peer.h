// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EXTENSION_LOCALIZATION_PEER_H_
#define CHROME_RENDERER_EXTENSIONS_EXTENSION_LOCALIZATION_PEER_H_

#include <string>

#include "content/public/child/request_peer.h"
#include "content/public/common/resource_response_info.h"

namespace IPC {
class Sender;
}

// The ExtensionLocalizationPeer is a proxy to a
// content::RequestPeer instance.  It is used to pre-process
// CSS files requested by extensions to replace localization templates with the
// appropriate localized strings.
//
// Call the factory method CreateExtensionLocalizationPeer() to obtain an
// instance of ExtensionLocalizationPeer based on the original Peer.
class ExtensionLocalizationPeer : public content::RequestPeer {
 public:
  virtual ~ExtensionLocalizationPeer();

  static ExtensionLocalizationPeer* CreateExtensionLocalizationPeer(
      content::RequestPeer* peer,
      IPC::Sender* message_sender,
      const std::string& mime_type,
      const GURL& request_url);

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

 private:
  friend class ExtensionLocalizationPeerTest;

  // Use CreateExtensionLocalizationPeer to create an instance.
  ExtensionLocalizationPeer(content::RequestPeer* peer,
                            IPC::Sender* message_sender,
                            const GURL& request_url);

  // Loads message catalogs, and replaces all __MSG_some_name__ templates within
  // loaded file.
  void ReplaceMessages();

  // Original peer that handles the request once we are done processing data_.
  content::RequestPeer* original_peer_;

  // We just pass though the response info. This holds the copy of the original.
  content::ResourceResponseInfo response_info_;

  // Sends ExtensionHostMsg_GetMessageBundle message to the browser to fetch
  // message catalog.
  IPC::Sender* message_sender_;

  // Buffer for incoming data. We wait until OnCompletedRequest before using it.
  std::string data_;

  // Original request URL.
  GURL request_url_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionLocalizationPeer);
};

#endif  // CHROME_RENDERER_EXTENSIONS_EXTENSION_LOCALIZATION_PEER_H_
