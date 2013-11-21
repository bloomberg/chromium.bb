// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MEDIA_CDM_JSON_WEB_KEY_H_
#define MEDIA_CDM_JSON_WEB_KEY_H_

#include <string>

#include "base/basictypes.h"
#include "media/base/media_export.h"

namespace media {

// Convert |key|, |key_id| to a JSON Web Key Set.
MEDIA_EXPORT std::string GenerateJWKSet(const uint8* key, int key_length,
                                        const uint8* key_id, int key_id_length);

}  // namespace media

#endif  // MEDIA_CDM_JSON_WEB_KEY_H_
