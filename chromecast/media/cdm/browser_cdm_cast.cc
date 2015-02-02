// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cdm/browser_cdm_cast.h"

#include "media/base/cdm_key_information.h"

namespace chromecast {
namespace media {

BrowserCdmCast::BrowserCdmCast() {
}

BrowserCdmCast::~BrowserCdmCast() {
  player_tracker_.NotifyCdmUnset();
}

void BrowserCdmCast::SetCallbacks(
    const ::media::SessionMessageCB& session_message_cb,
    const ::media::SessionClosedCB& session_closed_cb,
    const ::media::SessionErrorCB& session_error_cb,
    const ::media::SessionKeysChangeCB& session_keys_change_cb,
    const ::media::SessionExpirationUpdateCB& session_expiration_update_cb) {
  session_message_cb_ = session_message_cb;
  session_closed_cb_ = session_closed_cb;
  session_error_cb_ = session_error_cb;
  session_keys_change_cb_ = session_keys_change_cb;
  session_expiration_update_cb_ = session_expiration_update_cb;
}

int BrowserCdmCast::RegisterPlayer(const base::Closure& new_key_cb,
                                   const base::Closure& cdm_unset_cb) {
  return player_tracker_.RegisterPlayer(new_key_cb, cdm_unset_cb);
}

void BrowserCdmCast::UnregisterPlayer(int registration_id) {
  player_tracker_.UnregisterPlayer(registration_id);
}

void BrowserCdmCast::OnSessionMessage(const std::string& web_session_id,
                                      const std::vector<uint8>& message,
                                      const GURL& destination_url) {
  // Note: Message type is not supported in Chromecast. Do our best guess here.
  ::media::MediaKeys::MessageType message_type =
      destination_url.is_empty() ? ::media::MediaKeys::LICENSE_REQUEST
                                 : ::media::MediaKeys::LICENSE_RENEWAL;
  session_message_cb_.Run(web_session_id,
                          message_type,
                          message,
                          destination_url);
}

void BrowserCdmCast::OnSessionClosed(const std::string& web_session_id) {
  session_closed_cb_.Run(web_session_id);
}

void BrowserCdmCast::OnSessionKeysChange(
    const std::string& web_session_id,
    const ::media::KeyIdAndKeyPairs& keys) {
  ::media::CdmKeysInfo cdm_keys_info;
  for (const std::pair<std::string, std::string>& key : keys) {
    scoped_ptr< ::media::CdmKeyInformation> cdm_key_information(
        new ::media::CdmKeyInformation());
    cdm_key_information->key_id.assign(key.first.begin(), key.first.end());
    cdm_keys_info.push_back(cdm_key_information.release());
  }
  session_keys_change_cb_.Run(web_session_id, true, cdm_keys_info.Pass());

  // Notify listeners of a new key.
  player_tracker_.NotifyNewKey();
}

}  // namespace media
}  // namespace chromecast
