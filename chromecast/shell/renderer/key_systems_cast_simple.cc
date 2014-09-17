// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/shell/renderer/key_systems_cast.h"

namespace chromecast {
namespace shell {

void AddChromecastPlatformKeySystems(
    std::vector<content::KeySystemInfo>* key_systems_info) {
  // Intentional no-op for public build.
}

}  // namespace shell
}  // namespace chromecast
