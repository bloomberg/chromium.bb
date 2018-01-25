// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/cdm_context.h"

namespace media {

CdmContext::CdmContext() = default;

CdmContext::~CdmContext() = default;

void IgnoreCdmAttached(bool /* success */) {}

Decryptor* CdmContext::GetDecryptor() {
  return nullptr;
}

int CdmContext::GetCdmId() const {
  return kInvalidCdmId;
}

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
bool CdmContext::GetDecryptContext() {
  return false;
}
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

void* CdmContext::GetClassIdentifier() const {
  return nullptr;
}

}  // namespace media
