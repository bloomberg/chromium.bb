// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_factory.h"

#include <utility>

#include "base/bind.h"
#include "base/location.h"
#include "base/single_thread_task_runner.h"
#include "base/thread_task_runner_handle.h"
#include "media/base/key_systems.h"
#include "media/cdm/aes_decryptor.h"
#include "media/mojo/interfaces/service_factory.mojom.h"
#include "media/mojo/services/mojo_cdm.h"
#include "mojo/public/cpp/bindings/interface_request.h"
#include "mojo/shell/public/cpp/connect.h"

namespace media {

MojoCdmFactory::MojoCdmFactory(interfaces::ServiceFactory* service_factory)
    : service_factory_(service_factory) {
  DCHECK(service_factory_);
}

MojoCdmFactory::~MojoCdmFactory() {
}

void MojoCdmFactory::Create(
    const std::string& key_system,
    const GURL& security_origin,
    const CdmConfig& cdm_config,
    const SessionMessageCB& session_message_cb,
    const SessionClosedCB& session_closed_cb,
    const LegacySessionErrorCB& legacy_session_error_cb,
    const SessionKeysChangeCB& session_keys_change_cb,
    const SessionExpirationUpdateCB& session_expiration_update_cb,
    const CdmCreatedCB& cdm_created_cb) {
  DVLOG(2) << __FUNCTION__ << ": " << key_system;
  DCHECK(service_factory_);

  if (!security_origin.is_valid()) {
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(cdm_created_cb, nullptr, "Invalid origin."));
    return;
  }

  if (CanUseAesDecryptor(key_system)) {
    scoped_refptr<MediaKeys> cdm(
        new AesDecryptor(security_origin, session_message_cb, session_closed_cb,
                         session_keys_change_cb));
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(cdm_created_cb, cdm, ""));
    return;
  }

  interfaces::ContentDecryptionModulePtr cdm_ptr;
  service_factory_->CreateCdm(mojo::GetProxy(&cdm_ptr));

  MojoCdm::Create(key_system, security_origin, cdm_config, std::move(cdm_ptr),
                  session_message_cb, session_closed_cb,
                  legacy_session_error_cb, session_keys_change_cb,
                  session_expiration_update_cb, cdm_created_cb);
}

}  // namespace media
