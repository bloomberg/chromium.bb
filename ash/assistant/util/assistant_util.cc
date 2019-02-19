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

bool IsEmbeddedUiEntryPoint(AssistantEntryPoint entry_point) {
  return entry_point == AssistantEntryPoint::kDeepLink ||
         entry_point == AssistantEntryPoint::kHotkey ||
         entry_point == AssistantEntryPoint::kHotword ||
         entry_point == AssistantEntryPoint::kLauncherSearchBox ||
         entry_point == AssistantEntryPoint::kLauncherSearchResult ||
         entry_point == AssistantEntryPoint::kLongPressLauncher ||
         entry_point == AssistantEntryPoint::kUnspecified;
}

}  // namespace util
}  // namespace assistant
}  // namespace ash
