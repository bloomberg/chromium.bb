// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/icon_key_util.h"

namespace apps_util {

IncrementingIconKeyFactory::IncrementingIconKeyFactory() : u_key_(0) {}

apps::mojom::IconKeyPtr IncrementingIconKeyFactory::MakeIconKey(
    apps::mojom::AppType app_type,
    const std::string& s_key,
    uint8_t flags) {
  // The flags occupy the low 8 bits.
  u_key_ += 1 << 8;

  return apps::mojom::IconKey::New(
      app_type, u_key_ | static_cast<uint64_t>(flags), s_key);
}

}  // namespace apps_util
