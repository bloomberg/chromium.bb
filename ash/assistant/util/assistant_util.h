// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ASSISTANT_UTIL_ASSISTANT_UTIL_H_
#define ASH_ASSISTANT_UTIL_ASSISTANT_UTIL_H_

#include "base/component_export.h"

namespace ash {

enum class AssistantEntryPoint;
enum class AssistantVisibility;

namespace assistant {
namespace util {

// Returns true if Assistant is starting a new session, false otherwise.
COMPONENT_EXPORT(ASSISTANT_UTIL)
bool IsStartingSession(AssistantVisibility new_visibility,
                       AssistantVisibility old_visibility);

// Returns true if Assistant is finishing a session, false otherwise.
COMPONENT_EXPORT(ASSISTANT_UTIL)
bool IsFinishingSession(AssistantVisibility new_visibility);

// Returns true if the |entry_point| should show the embedded Assistant UI in
// Launcher.
COMPONENT_EXPORT(ASSISTANT_UTIL)
bool IsEmbeddedUiEntryPoint(AssistantEntryPoint entry_point);

}  // namespace util
}  // namespace assistant
}  // namespace ash

#endif  // ASH_ASSISTANT_UTIL_ASSISTANT_UTIL_H_
