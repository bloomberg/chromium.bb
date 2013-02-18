// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/password_generation_util.h"

#include "base/metrics/histogram.h"

namespace password_generation {

PasswordGenerationActions::PasswordGenerationActions()
    : learn_more_visited(false),
      password_accepted(false),
      password_edited(false),
      password_regenerated(false) {
}

PasswordGenerationActions::~PasswordGenerationActions() {
}

void LogUserActions(PasswordGenerationActions actions) {
  UserAction action = IGNORE_FEATURE;
  if (actions.password_accepted) {
    if (actions.password_edited)
      action = ACCEPT_AFTER_EDITING;
    else
      action = ACCEPT_ORIGINAL_PASSWORD;
  } else if (actions.learn_more_visited) {
    action = LEARN_MORE;
  }
  UMA_HISTOGRAM_ENUMERATION("PasswordGeneration.UserActions",
                            action, ACTION_ENUM_COUNT);
}

void LogPasswordGenerationEvent(PasswordGenerationEvent event) {
  UMA_HISTOGRAM_ENUMERATION("PasswordGeneration.Event",
                            event, EVENT_ENUM_COUNT);
}

}  // namespace password_generation
