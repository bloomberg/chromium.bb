// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/cdm/android_cdm_factory.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/location.h"
#include "base/thread_task_runner_handle.h"
#include "media/base/android/media_drm_bridge.h"
#include "media/base/key_systems.h"
#include "url/gurl.h"

namespace media {

AndroidCdmFactory::AndroidCdmFactory() {}

AndroidCdmFactory::~AndroidCdmFactory() {}

void AndroidCdmFactory::Create(
    const std::string& key_system,
    const GURL& security_origin,
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const LegacySessionErrorCB& legacy_session_error_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    const CdmCreatedCB& cdm_created_cb) {
  if (!security_origin.is_valid()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(cdm_created_cb, nullptr, "Invalid origin."));
    return;
  }

  if (!MediaDrmBridge::IsKeySystemSupported(key_system)) {
    NOTREACHED() << "Key system not supported unexpectedly: " << key_system;
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(cdm_created_cb, nullptr,
                              "Key system not supported unexpectedly."));
    return;
  }

  // TODO(xhwang): Currently MediaDrmBridge can only be created on the Browser
  // UI thread. Create it here after that is fixed. See http://crbug.com/546108
  NOTIMPLEMENTED();

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(cdm_created_cb, nullptr, "MediaDrmBridge cannot be created."));
}

}  // namespace media
