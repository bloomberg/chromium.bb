// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_CHOOSER_PROMPT_H_
#define CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_CHOOSER_PROMPT_H_

// A platform-independent interface for the account chooser dialog.
class AccountChooserPrompt {
 public:
  // Shows the account chooser dialog.
  virtual void Show() = 0;

  // Notifies the UI element that it's controller is no longer managing the UI
  // element. The dialog should close.
  virtual void ControllerGone() = 0;
 protected:
  virtual ~AccountChooserPrompt() = default;
};



#endif  // CHROME_BROWSER_UI_PASSWORDS_ACCOUNT_CHOOSER_PROMPT_H_
