// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_SUPPORTED_CDM_VERSIONS_H_
#define MEDIA_CDM_SUPPORTED_CDM_VERSIONS_H_

#include "media/base/media_export.h"

namespace media {

MEDIA_EXPORT bool IsSupportedCdmModuleVersion(int version);

MEDIA_EXPORT bool IsSupportedCdmInterfaceVersion(int version);

MEDIA_EXPORT bool IsSupportedCdmHostVersion(int version);

}  // namespace media

#endif  // MEDIA_CDM_SUPPORTED_CDM_VERSIONS_H_
