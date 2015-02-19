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

scoped_ptr<BrowserCdm> BrowserCdmFactoryAndroid::CreateBrowserCdm(
    const std::string& key_system,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionErrorCB& session_error_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb) {
  if (!MediaDrmBridge::IsKeySystemSupported(key_system)) {
    NOTREACHED() << "Unsupported key system: " << key_system;
    return scoped_ptr<BrowserCdm>();
  }

  scoped_ptr<MediaDrmBridge> cdm(MediaDrmBridge::Create(
      key_system, session_message_cb, session_closed_cb, session_error_cb,
      session_keys_change_cb, session_expiration_update_cb));
  if (!cdm) {
    NOTREACHED() << "MediaDrmBridge cannot be created for " << key_system;
    return scoped_ptr<BrowserCdm>();
  }

  // TODO(xhwang/ddorwin): Pass the security level from key system.
  // http://crbug.com/459400
  if (key_system == kWidevineKeySystem) {
    MediaDrmBridge::SecurityLevel security_level =
        MediaDrmBridge::SECURITY_LEVEL_3;
    if (base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kMediaDrmEnableNonCompositing)) {
      security_level = MediaDrmBridge::SECURITY_LEVEL_1;
    }
    if (!cdm->SetSecurityLevel(security_level)) {
      DVLOG(1) << "failed to set security level " << security_level;
      return scoped_ptr<BrowserCdm>();
    }
  } else {
    // Assume other key systems do not support full compositing.
    if (!base::CommandLine::ForCurrentProcess()->HasSwitch(
            switches::kMediaDrmEnableNonCompositing)) {
      NOTREACHED() << key_system << " may require "
                   << switches::kMediaDrmEnableNonCompositing;
      return scoped_ptr<BrowserCdm>();
    }
  }

  return cdm.Pass();
}

}  // namespace media
