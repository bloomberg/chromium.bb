// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/browser/media/cast_browser_cdm_factory.h"

namespace chromecast {
namespace media {

scoped_ptr< ::media::BrowserCdm> CreatePlatformBrowserCdm(
    const CastKeySystem& key_system,
    const ::media::SessionMessageCB& session_message_cb,
    const ::media::SessionClosedCB& session_closed_cb,
    const ::media::SessionErrorCB& session_error_cb,
    const ::media::SessionKeysChangeCB& session_keys_change_cb,
    const ::media::SessionExpirationUpdateCB& session_expiration_update_cb) {
  return scoped_ptr< ::media::BrowserCdm>();
}

}  // namespace media
}  // namespace chromecast
