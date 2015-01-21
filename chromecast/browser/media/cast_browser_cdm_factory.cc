// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/cast_browser_cdm_factory.h"

namespace chromecast {
namespace media {

scoped_ptr< ::media::BrowserCdm> CastBrowserCdmFactory::CreateBrowserCdm(
    const std::string& key_system_name,
    const ::media::SessionMessageCB& session_message_cb,
    const ::media::SessionClosedCB& session_closed_cb,
    const ::media::SessionErrorCB& session_error_cb,
    const ::media::SessionKeysChangeCB& session_keys_change_cb,
    const ::media::SessionExpirationUpdateCB& session_expiration_update_cb) {
  CastKeySystem key_system(GetKeySystemByName(key_system_name));

  // TODO(gunsch): handle ClearKey decryption. See crbug.com/441957

  scoped_ptr< ::media::BrowserCdm> platform_cdm(
      CreatePlatformBrowserCdm(key_system,
                               session_message_cb,
                               session_closed_cb,
                               session_error_cb,
                               session_keys_change_cb,
                               session_expiration_update_cb));
  if (platform_cdm) {
    return platform_cdm.Pass();
  }

  LOG(INFO) << "No matching key system found.";
  return scoped_ptr< ::media::BrowserCdm>();
}

}  // namespace media
}  // namespace chromecast
