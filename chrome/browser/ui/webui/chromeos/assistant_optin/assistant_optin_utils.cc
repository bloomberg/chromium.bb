// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/webui/chromeos/assistant_optin/assistant_optin_utils.h"

#include "base/metrics/histogram_macros.h"

namespace chromeos {

void RecordAssistantOptInStatus(AssistantOptInFlowStatus status) {
  UMA_HISTOGRAM_ENUMERATION("Assistant.OptInFlowStatus", status, kMaxValue + 1);
}

}  // namespace chromeos
