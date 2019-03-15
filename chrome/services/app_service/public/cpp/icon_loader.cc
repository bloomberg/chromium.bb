// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/services/app_service/public/cpp/icon_loader.h"

#include <utility>

#include "base/callback.h"

namespace apps {

IconLoader::Releaser::Releaser(std::unique_ptr<IconLoader::Releaser> next,
                               base::OnceClosure closure)
    : next_(std::move(next)), closure_(std::move(closure)) {}

IconLoader::Releaser::~Releaser() {
  std::move(closure_).Run();
}

IconLoader::Key::Key(const apps::mojom::IconKey& icon_key,
                     apps::mojom::IconCompression icon_compression,
                     int32_t size_hint_in_dip,
                     bool allow_placeholder_icon)
    : app_type_(icon_key.app_type),
      s_key_(icon_key.s_key),
      u_key_(icon_key.u_key),
      icon_effects_(icon_key.icon_effects),
      icon_compression_(icon_compression),
      size_hint_in_dip_(size_hint_in_dip),
      allow_placeholder_icon_(allow_placeholder_icon) {}

bool IconLoader::Key::operator<(const Key& that) const {
  if (this->app_type_ != that.app_type_) {
    return this->app_type_ < that.app_type_;
  }
  if (this->u_key_ != that.u_key_) {
    return this->u_key_ < that.u_key_;
  }
  if (this->icon_effects_ != that.icon_effects_) {
    return this->icon_effects_ < that.icon_effects_;
  }
  if (this->icon_compression_ != that.icon_compression_) {
    return this->icon_compression_ < that.icon_compression_;
  }
  if (this->size_hint_in_dip_ != that.size_hint_in_dip_) {
    return this->size_hint_in_dip_ < that.size_hint_in_dip_;
  }
  if (this->allow_placeholder_icon_ != that.allow_placeholder_icon_) {
    return this->allow_placeholder_icon_ < that.allow_placeholder_icon_;
  }
  return this->s_key_ < that.s_key_;
}

std::unique_ptr<IconLoader::Releaser> IconLoader::LoadIcon(
    const std::string& app_id,
    apps::mojom::IconCompression icon_compression,
    int32_t size_hint_in_dip,
    bool allow_placeholder_icon,
    apps::mojom::Publisher::LoadIconCallback callback) {
  return LoadIconFromIconKey(GetIconKey(app_id), icon_compression,
                             size_hint_in_dip, allow_placeholder_icon,
                             std::move(callback));
}

}  // namespace apps
