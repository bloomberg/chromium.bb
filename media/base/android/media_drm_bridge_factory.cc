// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/android/media_drm_bridge_factory.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "media/base/cdm_config.h"
#include "media/base/content_decryption_module.h"
#include "third_party/widevine/cdm/widevine_cdm_common.h"
#include "url/origin.h"

namespace media {

MediaDrmBridgeFactory::MediaDrmBridgeFactory(
    const CreateFetcherCB& create_fetcher_cb,
    const CreateStorageCB& create_storage_cb)
    : create_fetcher_cb_(create_fetcher_cb),
      create_storage_cb_(create_storage_cb),
      weak_factory_(this) {
  DCHECK(create_fetcher_cb_);
  DCHECK(create_storage_cb_);
}

MediaDrmBridgeFactory::~MediaDrmBridgeFactory() {
  if (cdm_created_cb_)
    std::move(cdm_created_cb_).Run(nullptr, "CDM creation aborted");
}

void MediaDrmBridgeFactory::Create(
    const std::string& key_system,
    const url::Origin& security_origin,
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    const CdmCreatedCB& cdm_created_cb) {
  DCHECK(MediaDrmBridge::IsKeySystemSupported(key_system));
  DCHECK(MediaDrmBridge::IsAvailable());
  DCHECK(!security_origin.unique());
  DCHECK(scheme_uuid_.empty()) << "This factory can only be used once.";

  scheme_uuid_ = MediaDrmBridge::GetUUID(key_system);
  DCHECK(!scheme_uuid_.empty());

  // Set security level.
  if (key_system == kWidevineKeySystem) {
    security_level_ = cdm_config.use_hw_secure_codecs
                          ? MediaDrmBridge::SECURITY_LEVEL_1
                          : MediaDrmBridge::SECURITY_LEVEL_3;
  } else if (!cdm_config.use_hw_secure_codecs) {
    // Assume other key systems require hardware-secure codecs and thus do not
    // support full compositing.
    auto error_message =
        key_system +
        " may require use_video_overlay_for_embedded_encrypted_video";
    NOTREACHED() << error_message;
    cdm_created_cb.Run(nullptr, error_message);
    return;
  }

  session_message_cb_ = session_message_cb;
  session_closed_cb_ = session_closed_cb;
  session_keys_change_cb_ = session_keys_change_cb;
  session_expiration_update_cb_ = session_expiration_update_cb;
  cdm_created_cb_ = cdm_created_cb;

  // MediaDrmStorage may be lazy created in MediaDrmStorageBridge.
  storage_ = std::make_unique<MediaDrmStorageBridge>();

  // TODO(xhwang): We should always try per-origin provisioning as long as it's
  // supported regardless of whether persistent license is enabled or not.
  if (!MediaDrmBridge::IsPersistentLicenseTypeSupported(key_system)) {
    std::move(cdm_created_cb_).Run(CreateMediaDrmBridge(""), "");
    return;
  }

  storage_->Initialize(
      create_storage_cb_,
      base::BindOnce(&MediaDrmBridgeFactory::OnStorageInitialized,
                     weak_factory_.GetWeakPtr()));
}

void MediaDrmBridgeFactory::OnStorageInitialized() {
  DCHECK(storage_);

  // MediaDrmStorageBridge should always return a valid origin ID after
  // initialize. Otherwise the pipe is broken and we should not create
  // MediaDrmBridge here.
  auto origin_id = storage_->origin_id();
  DVLOG(2) << __func__ << ": origin_id = " << origin_id;
  if (origin_id.empty()) {
    std::move(cdm_created_cb_).Run(nullptr, "Cannot fetch origin ID");
    return;
  }

  std::move(cdm_created_cb_).Run(CreateMediaDrmBridge(origin_id), "");
}

scoped_refptr<MediaDrmBridge> MediaDrmBridgeFactory::CreateMediaDrmBridge(
    const std::string& origin_id) {
  return MediaDrmBridge::CreateInternal(
      scheme_uuid_, security_level_, std::move(storage_), create_fetcher_cb_,
      session_message_cb_, session_closed_cb_, session_keys_change_cb_,
      session_expiration_update_cb_, origin_id);
}

}  // namespace media
