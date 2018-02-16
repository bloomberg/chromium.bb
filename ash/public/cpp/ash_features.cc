// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/public/cpp/ash_features.h"

namespace ash {
namespace features {

const base::Feature kAshNewSystemMenu{"AshNewSystemMenu",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

bool IsNewSystemMenuEnabled() {
  return base::FeatureList::IsEnabled(kAshNewSystemMenu);
}

}  // namespace features
}  // namespace ash
