// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_CHROMEOS_ASSISTANT_OPTIN_ASSISTANT_OPTIN_UTILS_H_
#define CHROME_BROWSER_UI_WEBUI_CHROMEOS_ASSISTANT_OPTIN_ASSISTANT_OPTIN_UTILS_H_

#include <string>

#include "base/callback.h"

namespace chromeos {

enum class AssistantOptInScreenExitCode {
  VALUE_PROP_SKIPPED = 0,
  VALUE_PROP_ACCEPTED = 1,
  THIRD_PARTY_CONTINUED = 2,
  EMAIL_OPTED_IN = 3,
  EMAIL_OPTED_OUT = 4,
  READY_SCREEN_CONTINUED = 5,
  EXIT_CODES_COUNT
};

// Type of Assistant opt-in flow status. This enum is used to back an UMA
// histogram and should be treated as append-only.
enum AssistantOptInFlowStatus {
  FLOW_STARTED = 0,
  ACTIVITY_CONTROL_SHOWN,
  ACTIVITY_CONTROL_ACCEPTED,
  ACTIVITY_CONTROL_SKIPPED,
  THIRD_PARTY_SHOWN,
  THIRD_PARTY_CONTINUED,
  GET_MORE_SHOWN,
  EMAIL_OPTED_IN,
  EMAIL_OPTED_OUT,
  GET_MORE_CONTINUED,
  READY_SCREEN_SHOWN,
  READY_SCREEN_CONTINUED,
  // Magic constant used by the histogram macros.
  kMaxValue = READY_SCREEN_CONTINUED
};

using OnAssistantOptInScreenExitCallback =
    base::OnceCallback<void(AssistantOptInScreenExitCode exit_code)>;

void RecordAssistantOptInStatus(AssistantOptInFlowStatus);

}  // namespace chromeos

#endif  // CHROME_BROWSER_UI_WEBUI_CHROMEOS_ASSISTANT_OPTIN_ASSISTANT_OPTIN_UTILS_H_
