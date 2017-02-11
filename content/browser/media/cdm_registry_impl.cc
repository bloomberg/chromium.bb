// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/cdm_registry_impl.h"

#include <stddef.h>

#include "content/public/common/cdm_info.h"
#include "content/public/common/content_client.h"

namespace content {

// static
CdmRegistry* CdmRegistry::GetInstance() {
  return CdmRegistryImpl::GetInstance();
}

// static
CdmRegistryImpl* CdmRegistryImpl::GetInstance() {
  static CdmRegistryImpl* registry = new CdmRegistryImpl();
  return registry;
}

CdmRegistryImpl::CdmRegistryImpl() {}

CdmRegistryImpl::~CdmRegistryImpl() {}

void CdmRegistryImpl::Init() {
  // Let embedders register CDMs.
  GetContentClient()->AddContentDecryptionModules(&cdms_, nullptr);
}

void CdmRegistryImpl::RegisterCdm(const CdmInfo& info) {
  // Always register new CDMs at the beginning of the list, so that
  // subsequent requests get the latest.
  cdms_.insert(cdms_.begin(), info);
}

const std::vector<CdmInfo>& CdmRegistryImpl::GetAllRegisteredCdms() {
  return cdms_;
}

}  // namespace media
