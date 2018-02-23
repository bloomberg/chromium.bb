// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_features.h"

namespace ash {
namespace features {

const base::Feature kSystemTrayUnified{"SystemTrayUnified",
                                       base::FEATURE_DISABLED_BY_DEFAULT};

bool IsSystemTrayUnifiedEnabled() {
  return base::FeatureList::IsEnabled(kSystemTrayUnified);
}

}  // namespace features
}  // namespace ash
