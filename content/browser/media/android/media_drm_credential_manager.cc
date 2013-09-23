// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/media_drm_credential_manager.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "media/base/android/media_drm_bridge.h"
#include "url/gurl.h"

namespace content {

// TODO(qinmin): Move the UUID definition to some common places.
static const uint8 kWidevineUuid[16] = {
    0xED, 0xEF, 0x8B, 0xA9, 0x79, 0xD6, 0x4A, 0xCE,
    0xA3, 0xC8, 0x27, 0xDC, 0xD5, 0x1D, 0x21, 0xED };

MediaDrmCredentialManager::MediaDrmCredentialManager() {}

MediaDrmCredentialManager::~MediaDrmCredentialManager() {}

void MediaDrmCredentialManager::ResetCredentials(
    const ResetCredentialsCB& callback) {
  // Ignore reset request if one is already in progress.
  if (!reset_credentials_cb_.is_null())
    return;

  reset_credentials_cb_ = callback;

  // First reset the L3 credential.
  if (!ResetCredentialsInternal("L3")) {
    // TODO(qinmin): We should post a task instead.
    base::ResetAndReturn(&reset_credentials_cb_).Run(false);
  }
}

void MediaDrmCredentialManager::OnResetCredentialsCompleted(
    const std::string& security_level, bool success) {
  if (security_level == "L3" && success) {
    if (ResetCredentialsInternal("L1"))
      return;
    success = false;
  }

  base::ResetAndReturn(&reset_credentials_cb_).Run(success);
  media_drm_bridge_.reset();
}

bool MediaDrmCredentialManager::ResetCredentialsInternal(
    const std::string& security_level) {
  std::vector<uint8> uuid(kWidevineUuid, kWidevineUuid + 16);
  media_drm_bridge_ = media::MediaDrmBridge::Create(
      0, uuid, GURL(), security_level, NULL);
  if (!media_drm_bridge_)
    return false;
  media_drm_bridge_->ResetDeviceCredentials(
      base::Bind(&MediaDrmCredentialManager::OnResetCredentialsCompleted,
                 base::Unretained(this), security_level));
  return true;
}

}  // namespace content
