// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMECAST_RENDERER_KEY_SYSTEMS_CAST_H_
#define CHROMECAST_RENDERER_KEY_SYSTEMS_CAST_H_

#include <vector>

#include "media/base/key_system_info.h"

namespace chromecast {
namespace shell {

// Adds a single key system by name.
// TODO(gunsch): modify this API to accept specifying different supported
// features, and/or APIs per key system type.
void AddKeySystemWithCodecs(
    const std::string& key_system_name,
    std::vector<::media::KeySystemInfo>* concrete_key_systems);

void AddChromecastKeySystems(
    std::vector<::media::KeySystemInfo>* key_systems_info);

}  // namespace shell
}  // namespace chromecast

#endif  // CHROMECAST_RENDERER_KEY_SYSTEMS_CAST_H_
