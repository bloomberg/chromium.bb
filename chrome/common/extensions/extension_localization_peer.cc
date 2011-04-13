// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension_localization_peer.h"

#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/common/extensions/extension_message_bundle.h"
#include "chrome/common/extensions/extension_messages.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"
#include "webkit/glue/webkit_glue.h"

ExtensionLocalizationPeer::ExtensionLocalizationPeer(
    webkit_glue::ResourceLoaderBridge::Peer* peer,
    IPC::Message::Sender* message_sender,
    const GURL& request_url)
    : original_peer_(peer),
      message_sender_(message_sender),
      request_url_(request_url) {
}

ExtensionLocalizationPeer::~ExtensionLocalizationPeer() {
}

// static
ExtensionLocalizationPeer*
ExtensionLocalizationPeer::CreateExtensionLocalizationPeer(
    webkit_glue::ResourceLoaderBridge::Peer* peer,
    IPC::Message::Sender* message_sender,
    const std::string& mime_type,
    const GURL& request_url) {
  // Return NULL if content is not text/css or it doesn't belong to extension
  // scheme.
  return (request_url.SchemeIs(chrome::kExtensionScheme) &&
          StartsWithASCII(mime_type, "text/css", false)) ?
      new ExtensionLocalizationPeer(peer, message_sender, request_url) : NULL;
}

void ExtensionLocalizationPeer::OnUploadProgress(
    uint64 position, uint64 size) {
  NOTREACHED();
}

bool ExtensionLocalizationPeer::OnReceivedRedirect(
    const GURL& new_url,
    const webkit_glue::ResourceResponseInfo& info,
    bool* has_new_first_party_for_cookies,
    GURL* new_first_party_for_cookies) {
  NOTREACHED();
  return false;
}

void ExtensionLocalizationPeer::OnReceivedResponse(
    const webkit_glue::ResourceResponseInfo& info) {
  response_info_ = info;
}

void ExtensionLocalizationPeer::OnReceivedData(const char* data,
                                               int data_length,
                                               int encoded_data_length) {
  data_.append(data, data_length);
}

void ExtensionLocalizationPeer::OnCompletedRequest(
    const net::URLRequestStatus& status,
    const std::string& security_info,
    const base::Time& completion_time) {
  // Make sure we delete ourselves at the end of this call.
  scoped_ptr<ExtensionLocalizationPeer> this_deleter(this);

  // Give sub-classes a chance at altering the data.
  if (status.status() != net::URLRequestStatus::SUCCESS) {
    // We failed to load the resource.
    original_peer_->OnReceivedResponse(response_info_);
    net::URLRequestStatus status(net::URLRequestStatus::CANCELED,
                                 net::ERR_ABORTED);
    original_peer_->OnCompletedRequest(status, security_info, completion_time);
    return;
  }

  ReplaceMessages();

  original_peer_->OnReceivedResponse(response_info_);
  if (!data_.empty())
    original_peer_->OnReceivedData(data_.data(),
                                   static_cast<int>(data_.size()),
                                   -1);
  original_peer_->OnCompletedRequest(status, security_info, completion_time);
}

void ExtensionLocalizationPeer::ReplaceMessages() {
  if (!message_sender_ || data_.empty())
    return;

  if (!request_url_.is_valid())
    return;

  std::string extension_id = request_url_.host();
  L10nMessagesMap* l10n_messages = GetL10nMessagesMap(extension_id);
  if (!l10n_messages) {
    L10nMessagesMap messages;
    message_sender_->Send(new ExtensionHostMsg_GetMessageBundle(
        extension_id, &messages));

    // Save messages we got, so we don't have to ask again.
    // Messages map is never empty, it contains at least @@extension_id value.
    ExtensionToL10nMessagesMap& l10n_messages_map =
        *GetExtensionToL10nMessagesMap();
    l10n_messages_map[extension_id] = messages;

    l10n_messages = GetL10nMessagesMap(extension_id);
  }

  std::string error;
  if (ExtensionMessageBundle::ReplaceMessagesWithExternalDictionary(
          *l10n_messages, &data_, &error)) {
    data_.resize(data_.size());
  }
}
