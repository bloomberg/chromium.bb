// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_AUDIO_DECODER_MANIFEST_H_
#define CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_AUDIO_DECODER_MANIFEST_H_

#include "services/service_manager/public/cpp/manifest.h"

namespace chromeos {
namespace assistant {

const service_manager::Manifest& GetAudioDecoderManifest();

}  // namespace assistant
}  // namespace chromeos

#endif  // CHROMEOS_SERVICES_ASSISTANT_PUBLIC_CPP_AUDIO_DECODER_MANIFEST_H_
