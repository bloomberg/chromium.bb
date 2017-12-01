// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_service_context.h"

#include "base/bind.h"
#include "base/logging.h"
#include "media/base/content_decryption_module.h"
#include "media/mojo/services/mojo_cdm_service.h"

namespace media {

MojoCdmServiceContext::MojoCdmServiceContext() = default;

MojoCdmServiceContext::~MojoCdmServiceContext() = default;

void MojoCdmServiceContext::RegisterCdm(int cdm_id,
                                        MojoCdmService* cdm_service) {
  DCHECK(!cdm_services_.count(cdm_id));
  DCHECK(cdm_service);
  cdm_services_[cdm_id] = cdm_service;
}

void MojoCdmServiceContext::UnregisterCdm(int cdm_id) {
  DCHECK(cdm_services_.count(cdm_id));
  cdm_services_.erase(cdm_id);
}

scoped_refptr<ContentDecryptionModule> MojoCdmServiceContext::GetCdm(
    int cdm_id) {
  auto cdm_service = cdm_services_.find(cdm_id);
  if (cdm_service == cdm_services_.end()) {
    LOG(ERROR) << "CDM service not found: " << cdm_id;
    return nullptr;
  }

  return cdm_service->second->GetCdm();
}

}  // namespace media
