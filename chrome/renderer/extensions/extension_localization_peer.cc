// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/renderer/extensions/extension_localization_peer.h"

#include "base/memory/scoped_ptr.h"
#include "base/strings/string_util.h"
#include "chrome/common/url_constants.h"
#include "extensions/common/constants.h"
#include "extensions/common/extension_messages.h"
#include "extensions/common/message_bundle.h"
#include "ipc/ipc_sender.h"
#include "net/base/net_errors.h"
#include "net/http/http_response_headers.h"

namespace {

class StringData final : public content::RequestPeer::ReceivedData {
 public:
  explicit StringData(const std::string& data) : data_(data) {}
  void Append(const char* data, int length) { data_.append(data, length); }

  const char* payload() const override { return data_.data(); }
  int length() const override { return data_.size(); }
  int encoded_length() const override { return -1; }

 private:
  std::string data_;

  DISALLOW_COPY_AND_ASSIGN(StringData);
};

}  // namespace

ExtensionLocalizationPeer::ExtensionLocalizationPeer(
    content::RequestPeer* peer,
    IPC::Sender* message_sender,
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
    content::RequestPeer* peer,
    IPC::Sender* message_sender,
    const std::string& mime_type,
    const GURL& request_url) {
  // Return NULL if content is not text/css or it doesn't belong to extension
  // scheme.
  return (request_url.SchemeIs(extensions::kExtensionScheme) &&
          base::StartsWith(mime_type, "text/css",
                           base::CompareCase::INSENSITIVE_ASCII))
             ? new ExtensionLocalizationPeer(peer, message_sender, request_url)
             : NULL;
}

void ExtensionLocalizationPeer::OnUploadProgress(
    uint64 position, uint64 size) {
  NOTREACHED();
}

bool ExtensionLocalizationPeer::OnReceivedRedirect(
    const net::RedirectInfo& redirect_info,
    const content::ResourceResponseInfo& info) {
  NOTREACHED();
  return false;
}

void ExtensionLocalizationPeer::OnReceivedResponse(
    const content::ResourceResponseInfo& info) {
  response_info_ = info;
}

void ExtensionLocalizationPeer::OnReceivedData(scoped_ptr<ReceivedData> data) {
  data_.append(data->payload(), data->length());
}

void ExtensionLocalizationPeer::OnCompletedRequest(
    int error_code,
    bool was_ignored_by_handler,
    bool stale_copy_in_cache,
    const std::string& security_info,
    const base::TimeTicks& completion_time,
    int64 total_transfer_size) {
  // Make sure we delete ourselves at the end of this call.
  scoped_ptr<ExtensionLocalizationPeer> this_deleter(this);
  // Give sub-classes a chance at altering the data.
  if (error_code != net::OK) {
    // We failed to load the resource.
    original_peer_->OnReceivedCompletedResponse(
        response_info_, nullptr, net::ERR_ABORTED, false, stale_copy_in_cache,
        security_info, completion_time, total_transfer_size);
    return;
  }

  ReplaceMessages();

  scoped_ptr<StringData> data_to_pass(data_.empty() ? nullptr
                                                    : new StringData(data_));
  original_peer_->OnReceivedCompletedResponse(
      response_info_, data_to_pass.Pass(), error_code, was_ignored_by_handler,
      stale_copy_in_cache, security_info, completion_time, total_transfer_size);
}

void ExtensionLocalizationPeer::OnReceivedCompletedResponse(
    const content::ResourceResponseInfo& info,
    scoped_ptr<ReceivedData> data,
    int error_code,
    bool was_ignored_by_handler,
    bool stale_copy_in_cache,
    const std::string& security_info,
    const base::TimeTicks& completion_time,
    int64 total_transfer_size) {
  // Make sure we delete ourselves at the end of this call.
  scoped_ptr<ExtensionLocalizationPeer> this_deleter(this);
  original_peer_->OnReceivedCompletedResponse(
      info, data.Pass(), error_code, was_ignored_by_handler,
      stale_copy_in_cache, security_info, completion_time, total_transfer_size);
}

void ExtensionLocalizationPeer::ReplaceMessages() {
  if (!message_sender_ || data_.empty())
    return;

  if (!request_url_.is_valid())
    return;

  std::string extension_id = request_url_.host();
  extensions::L10nMessagesMap* l10n_messages =
      extensions::GetL10nMessagesMap(extension_id);
  if (!l10n_messages) {
    extensions::L10nMessagesMap messages;
    message_sender_->Send(new ExtensionHostMsg_GetMessageBundle(
        extension_id, &messages));

    // Save messages we got, so we don't have to ask again.
    // Messages map is never empty, it contains at least @@extension_id value.
    extensions::ExtensionToL10nMessagesMap& l10n_messages_map =
        *extensions::GetExtensionToL10nMessagesMap();
    l10n_messages_map[extension_id] = messages;

    l10n_messages = extensions::GetL10nMessagesMap(extension_id);
  }

  std::string error;
  if (extensions::MessageBundle::ReplaceMessagesWithExternalDictionary(
          *l10n_messages, &data_, &error)) {
    data_.resize(data_.size());
  }
}
