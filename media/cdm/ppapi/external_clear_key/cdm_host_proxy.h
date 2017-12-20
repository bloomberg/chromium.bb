// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_CDM_HOST_PROXY_H_
#define MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_CDM_HOST_PROXY_H_

#include "media/cdm/api/content_decryption_module.h"

namespace media {

// An interface to proxy calls to the CDM Host.
class CdmHostProxy {
 public:
  virtual ~CdmHostProxy() = default;

  virtual cdm::Buffer* Allocate(uint32_t capacity) = 0;
  virtual cdm::FileIO* CreateFileIO(cdm::FileIOClient* client) = 0;
};

}  // namespace media

#endif  // MEDIA_CDM_PPAPI_EXTERNAL_CLEAR_KEY_CDM_HOST_PROXY_H_
