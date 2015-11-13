// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_factory.h"

#include "media/mojo/interfaces/service_factory.mojom.h"
#include "media/mojo/services/mojo_cdm.h"
#include "mojo/application/public/cpp/connect.h"
#include "mojo/public/cpp/bindings/interface_request.h"

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

  interfaces::ContentDecryptionModulePtr cdm_ptr;
  service_factory_->CreateCdm(mojo::GetProxy(&cdm_ptr));

  MojoCdm::Create(key_system, security_origin, cdm_config, cdm_ptr.Pass(),
                  session_message_cb, session_closed_cb,
                  legacy_session_error_cb, session_keys_change_cb,
                  session_expiration_update_cb, cdm_created_cb);
}

}  // namespace media
