// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/vr/assets.h"

#include "base/memory/singleton.h"
#include "base/values.h"

namespace vr {

struct AssetsSingletonTrait : public base::DefaultSingletonTraits<Assets> {
  static Assets* New() { return new Assets(); }
  static void Delete(Assets* assets) { delete assets; }
};

// static
Assets* Assets::GetInstance() {
  return base::Singleton<Assets, AssetsSingletonTrait>::get();
}

void Assets::OnComponentReady(const base::Version& version,
                              const base::FilePath& install_dir,
                              std::unique_ptr<base::DictionaryValue> manifest) {
  // TODO(706150): Make assets available to VR.
}

Assets::Assets() = default;

Assets::~Assets() = default;

}  // namespace vr
