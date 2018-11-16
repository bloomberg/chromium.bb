// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/public/features.h"

#include "base/feature_list.h"

namespace chromeos {
namespace assistant {
namespace features {

const base::Feature kAssistantVoiceMatch{"AssistantVoiceMatch",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

const base::Feature kEnableDspHotword{"EnableDspHotword",
                                      base::FEATURE_DISABLED_BY_DEFAULT};

bool IsDspHotwordEnabled() {
  return base::FeatureList::IsEnabled(kEnableDspHotword);
}

}  // namespace features
}  // namespace assistant
}  // namespace chromeos
