// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STATE_H_
#define COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STATE_H_

namespace autofill_assistant {

// High-level states the Autofill Assistant can be in.
enum class AutofillAssistantState {
  // Autofill assistant is not doing or showing anything.
  // Initial state.
  INACTIVE = 0,

  // Autofill assistant is waiting for an autostart script.
  //
  // Status message, progress and details are initialized to useful values.
  STARTING,

  // Autofill assistant is manipulating the website.
  //
  // Status message, progress and details kept up-to-date by the running
  // script.
  RUNNING,

  // Autofill assistant is waiting for the user to make a choice.
  //
  // Status message is initialized to a useful value. Chips are set and might be
  // empty. A touchable area must be configured. The user might be filling in
  // the data for a payment request.
  PROMPT,

  // Autofill assistant is waiting for the user to make the first choice.
  //
  // When autostartable scripts are expected, this is only triggered as a
  // fallback if there are non-autostartable scripts to choose from instead.
  AUTOSTART_FALLBACK_PROMPT,

  // Autofill assistant is expecting a modal dialog, such as the one asking for
  // CVC.
  MODAL_DIALOG,

  // Autofill assistant is stopped, but still visible to the user.
  //
  // Status message contains the final message.
  STOPPED
};

}  // namespace autofill_assistant

#endif  // COMPONENTS_AUTOFILL_ASSISTANT_BROWSER_STATE_H_
