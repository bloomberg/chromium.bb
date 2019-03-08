// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/assistant/util/assistant_util.h"

#include "ash/assistant/model/assistant_ui_model.h"

namespace ash {
namespace assistant {
namespace util {

bool IsStartingSession(AssistantVisibility new_visibility,
                       AssistantVisibility old_visibility) {
  return old_visibility == AssistantVisibility::kClosed &&
         new_visibility == AssistantVisibility::kVisible;
}

bool IsFinishingSession(AssistantVisibility new_visibility) {
  return new_visibility == AssistantVisibility::kClosed;
}

bool IsVoiceEntryPoint(AssistantEntryPoint entry_point, bool prefer_voice) {
  switch (entry_point) {
    case AssistantEntryPoint::kHotword:
    case AssistantEntryPoint::kLauncherSearchBoxMic:
      return true;
    case AssistantEntryPoint::kHotkey:
    case AssistantEntryPoint::kLauncherSearchBox:
    case AssistantEntryPoint::kLongPressLauncher:
      return prefer_voice;
    case AssistantEntryPoint::kUnspecified:
    case AssistantEntryPoint::kDeepLink:
    case AssistantEntryPoint::kLauncherSearchResult:
    case AssistantEntryPoint::kSetup:
    case AssistantEntryPoint::kStylus:
      return false;
  }
}

bool ShouldAttemptWarmerWelcome(AssistantEntryPoint entry_point) {
  switch (entry_point) {
    case AssistantEntryPoint::kDeepLink:
    case AssistantEntryPoint::kHotword:
    case AssistantEntryPoint::kLauncherSearchBoxMic:
    case AssistantEntryPoint::kLauncherSearchResult:
    case AssistantEntryPoint::kStylus:
      return false;
    case AssistantEntryPoint::kUnspecified:
    case AssistantEntryPoint::kHotkey:
    case AssistantEntryPoint::kLauncherSearchBox:
    case AssistantEntryPoint::kLongPressLauncher:
    case AssistantEntryPoint::kSetup:
      return true;
  }
}

}  // namespace util
}  // namespace assistant
}  // namespace ash
