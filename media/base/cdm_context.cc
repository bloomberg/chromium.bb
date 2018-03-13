// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/cdm_context.h"

namespace media {

CdmContext::CdmContext() = default;

CdmContext::~CdmContext() = default;

Decryptor* CdmContext::GetDecryptor() {
  return nullptr;
}

int CdmContext::GetCdmId() const {
  return kInvalidCdmId;
}

#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
CdmProxyContext* CdmContext::GetCdmProxyContext() {
  return nullptr;
}
#endif  // BUILDFLAG(ENABLE_LIBRARY_CDMS)

#if defined(OS_ANDROID)
MediaCryptoContext* CdmContext::GetMediaCryptoContext() {
  return nullptr;
}
#endif

void* CdmContext::GetClassIdentifier() const {
  return nullptr;
}

void IgnoreCdmAttached(bool /* success */) {}

}  // namespace media
