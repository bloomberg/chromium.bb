// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/browser_cdm_factory_android.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/media_switches.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"

namespace media {

ScopedBrowserCdmPtr BrowserCdmFactoryAndroid::CreateBrowserCdm(
    const std::string& key_system,
    bool use_hw_secure_codecs,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const LegacySessionErrorCB& legacy_session_error_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb) {
  if (!MediaDrmBridge::IsKeySystemSupported(key_system)) {
    NOTREACHED() << "Key system not supported unexpectedly: " << key_system;
    return ScopedBrowserCdmPtr();
  }

  ScopedMediaDrmBridgePtr cdm(
      MediaDrmBridge::Create(key_system, session_message_cb, session_closed_cb,
                             legacy_session_error_cb, session_keys_change_cb,
                             session_expiration_update_cb));
  if (!cdm) {
    NOTREACHED() << "MediaDrmBridge cannot be created for " << key_system;
    return ScopedBrowserCdmPtr();
  }

  if (key_system == kWidevineKeySystem) {
    MediaDrmBridge::SecurityLevel security_level =
        use_hw_secure_codecs ? MediaDrmBridge::SECURITY_LEVEL_1
                          : MediaDrmBridge::SECURITY_LEVEL_3;
    if (!cdm->SetSecurityLevel(security_level)) {
      DVLOG(1) << "failed to set security level " << security_level;
      return ScopedBrowserCdmPtr();
    }
  } else {
    // Assume other key systems require hardware-secure codecs and thus do not
    // support full compositing.
    if (!use_hw_secure_codecs) {
      NOTREACHED()
          << key_system
          << " may require use_video_overlay_for_embedded_encrypted_video";
      return ScopedBrowserCdmPtr();
    }
  }

  return cdm.Pass();
}

}  // namespace media
