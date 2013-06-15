// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_COMMON_PASSWORD_GENERATION_UTIL_H_
#define COMPONENTS_AUTOFILL_COMMON_PASSWORD_GENERATION_UTIL_H_

namespace autofill {
namespace password_generation {

// Enumerates various events related to the password generation process.
enum PasswordGenerationEvent {
  // No Account creation form is detected.
  NO_SIGN_UP_DETECTED,

  // Account creation form is detected.
  SIGN_UP_DETECTED,

  // Password generation icon is shown inside the first password field.
  ICON_SHOWN,

  // Password generation bubble is shown after user clicks on the icon.
  BUBBLE_SHOWN,

  // Number of enum entries, used for UMA histogram reporting macros.
  EVENT_ENUM_COUNT
};

// Wrapper to store the user interactions with the password generation bubble.
struct PasswordGenerationActions {
  // Whether the user has clicked on the learn more link.
  bool learn_more_visited;

  // Whether the user has accepted the generated password.
  bool password_accepted;

  // Whether the user has manually edited password entry.
  bool password_edited;

  // Whether the user has clicked on the regenerate button.
  bool password_regenerated;

  PasswordGenerationActions();
  ~PasswordGenerationActions();
};

void LogUserActions(PasswordGenerationActions actions);

void LogPasswordGenerationEvent(PasswordGenerationEvent event);

// Enumerates user actions after password generation bubble is shown.
// These are visible for testing purposes.
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

}  // namespace password_generation
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_COMMON_PASSWORD_GENERATION_UTIL_H_
