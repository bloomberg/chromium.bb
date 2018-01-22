// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_cdm_service_context.h"

#include "base/logging.h"
#include "media/base/content_decryption_module.h"
#include "media/cdm/cdm_context_ref_impl.h"
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

std::unique_ptr<CdmContextRef> MojoCdmServiceContext::GetCdmContextRef(
    int cdm_id) {
  auto cdm_service = cdm_services_.find(cdm_id);
  if (cdm_service == cdm_services_.end()) {
    // This could happen when called after the actual CDM has been destroyed.
    DVLOG(1) << "CDM service not found: " << cdm_id;
    return nullptr;
  }

  if (!cdm_service->second->GetCdm()->GetCdmContext()) {
    NOTREACHED() << "All CDMs should support CdmContext.";
    return nullptr;
  }

  return std::make_unique<CdmContextRefImpl>(cdm_service->second->GetCdm());
}

}  // namespace media
