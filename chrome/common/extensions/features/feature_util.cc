// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/features/feature_util.h"

#include "chrome/common/extensions/features/feature_channel.h"
#include "components/version_info/version_info.h"

namespace extensions {
namespace feature_util {

bool ExtensionServiceWorkersEnabled() {
  return true;
}

}  // namespace feature_util
}  // namespace extensions
