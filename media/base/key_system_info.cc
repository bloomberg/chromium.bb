// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/key_system_info.h"

namespace media {

KeySystemInfo::KeySystemInfo(const std::string& key_system)
    : key_system(key_system),
      supported_init_data_types(EME_INIT_DATA_TYPE_NONE),
      supported_codecs(EME_CODEC_NONE),
      use_aes_decryptor(false) {
}

KeySystemInfo::~KeySystemInfo() {
}

}  // namespace media
