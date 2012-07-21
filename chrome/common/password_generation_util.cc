// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/password_generation_util.h"

#include "base/metrics/histogram.h"

namespace {

// Enumerates user actions after password generation bubble is shown.
enum UserAction {
  // User closes the bubble without any meaningful actions (e.g. use backspace
  // key, close the bubble, click outside the bubble, etc).
  IGNORE_FEATURE,

  // User navigates to the learn more page. Note that in the current
  // implementation this will result in closing the bubble so this action
  // doesn't overlap with the following two actions.
  LEARN_MORE,

  // User accepts the generated password without manually editing it (but
  // including changing it through the regenerate button).
  ACCEPT_ORIGINAL_PASSWORD,

  // User accepts the gererated password after manually editing it.
  ACCEPT_AFTER_EDITING,

  // Number of enum entries, used for UMA histogram reporting macros.
  ACTION_ENUM_COUNT
};

}  // anonymous namespace

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
