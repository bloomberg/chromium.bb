// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/services/assistant/public/features.h"

namespace chromeos {
namespace assistant {
namespace features {

// Enables Assistant voice match enrollment.
const base::Feature kAssistantVoiceMatch{"AssistantVoiceMatch",
                                         base::FEATURE_DISABLED_BY_DEFAULT};

}  // namespace features
}  // namespace assistant
}  // namespace chromeos
