// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/key_system_info.h"

namespace media {

KeySystemInfo::KeySystemInfo()
    : supported_init_data_types(kInitDataTypeMaskNone),
      supported_codecs(EME_CODEC_NONE),
      max_audio_robustness(EmeRobustness::INVALID),
      max_video_robustness(EmeRobustness::INVALID),
      persistent_license_support(EmeSessionTypeSupport::INVALID),
      persistent_release_message_support(EmeSessionTypeSupport::INVALID),
      persistent_state_support(EmeFeatureSupport::INVALID),
      distinctive_identifier_support(EmeFeatureSupport::INVALID),
      use_aes_decryptor(false) {
}

KeySystemInfo::~KeySystemInfo() {
}

}  // namespace media
