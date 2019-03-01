// Copyright (c) 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/icon_key_util.h"

namespace apps_util {

IncrementingIconKeyFactory::IncrementingIconKeyFactory() : u_key_(0) {}

apps::mojom::IconKeyPtr IncrementingIconKeyFactory::MakeIconKey(
    apps::mojom::AppType app_type,
    apps::mojom::IconType icon_type,
    const std::string& s_key,
    uint8_t flags) {
  // The flags occupy the low 8 bits.
  u_key_ += 1 << 8;

  auto icon_key = apps::mojom::IconKey::New();
  icon_key->app_type = app_type;
  icon_key->icon_type = icon_type;
  icon_key->s_key = s_key;
  icon_key->u_key = u_key_ | static_cast<uint64_t>(flags);
  return icon_key;
}

}  // namespace apps_util
