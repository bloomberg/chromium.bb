// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_service_context.h"

#include "base/bind.h"
#include "base/logging.h"

namespace media {

MojoCdmServiceContext::MojoCdmServiceContext() {
}

MojoCdmServiceContext::~MojoCdmServiceContext() {
}

void MojoCdmServiceContext::CreateCdmService(
    const mojo::String& key_system,
    const mojo::String& security_origin,
    int32_t cdm_id,
    mojo::InterfaceRequest<mojo::ContentDecryptionModule> request) {
  DVLOG(1) << __FUNCTION__ << ": " << key_system;

  // TODO(xhwang): pass |security_origin| down because CdmFactory needs it.
  scoped_ptr<MojoCdmService> cdm_service =
      MojoCdmService::Create(key_system, this, request.Pass());
  if (cdm_service)
    services_.add(cdm_id, cdm_service.Pass());
}

CdmContext* MojoCdmServiceContext::GetCdmContext(int32_t cdm_id) {
  MojoCdmService* service = services_.get(cdm_id);
  if (!service) {
    LOG(ERROR) << "CDM context not found: " << cdm_id;
    return nullptr;
  }

  return service->GetCdmContext();
}

void MojoCdmServiceContext::ServiceHadConnectionError(MojoCdmService* service) {
  for (auto it = services_.begin(); it != services_.end(); ++it) {
    if (it->second == service) {
      services_.erase(it);  // This destroys the |service|.
      return;
    }
  }

  NOTREACHED() << "Service " << service << " not found.";
}

}  // namespace media
