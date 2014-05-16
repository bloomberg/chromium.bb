// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/cdm_factory.h"

#include "base/command_line.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/media_switches.h"

namespace media {

scoped_ptr<MediaKeys> CreateBrowserCdm(
    const std::string& key_system,
    const SessionCreatedCB& session_created_cb,
    const SessionMessageCB& session_message_cb,
    const SessionReadyCB& session_ready_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionErrorCB& session_error_cb) {
  if (!MediaDrmBridge::IsKeySystemSupported(key_system)) {
    NOTREACHED() << "Unsupported key system: " << key_system;
    return scoped_ptr<MediaKeys>();
  }

  scoped_ptr<MediaDrmBridge> cdm(MediaDrmBridge::Create(key_system,
                                                        session_created_cb,
                                                        session_message_cb,
                                                        session_ready_cb,
                                                        session_closed_cb,
                                                        session_error_cb));
  if (!cdm) {
    NOTREACHED() << "MediaDrmBridge cannot be created for " << key_system;
    return scoped_ptr<MediaKeys>();
  }

  // TODO(xhwang/ddorwin): Pass the security level from key system.
  MediaDrmBridge::SecurityLevel security_level =
      MediaDrmBridge::SECURITY_LEVEL_3;
  if (CommandLine::ForCurrentProcess()->HasSwitch(
          switches::kMediaDrmEnableNonCompositing)) {
    security_level = MediaDrmBridge::SECURITY_LEVEL_1;
  }
  if (!cdm->SetSecurityLevel(security_level)) {
    DVLOG(1) << "failed to set security level " << security_level;
    return scoped_ptr<MediaKeys>();
  }

  return cdm.PassAs<MediaKeys>();
}

}  // namespace media
