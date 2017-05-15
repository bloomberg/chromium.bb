// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_DIALOG_CONTROLLER_WIN_H_
#define CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_DIALOG_CONTROLLER_WIN_H_

#include <vector>

#include "base/macros.h"
#include "base/strings/string16.h"

namespace safe_browsing {

// Provides the various elements that will be displayed in the Chrome Cleaner
// UI. Also provides functions, such as |Accept()| and |Cancel()|, that should
// be called by the UI in response to user actions.
//
// This class manages its own lifetime and will delete itself once the Cleaner
// dialog has been dismissed and either of |Accept()| or |Cancel()| have been
// called.
class ChromeCleanerDialogController {
 public:
  ChromeCleanerDialogController();

  base::string16 GetWindowTitle() const;
  base::string16 GetMainText() const;
  base::string16 GetAcceptButtonLabel() const;
  base::string16 GetDetailsButtonLabel() const;

  // Called by the Cleaner dialog when the dialog has been shown. Used for
  // reporting metrics.
  void DialogShown();
  // Called by the Cleaner dialog when user accepts the prompt. Once |Accept()|
  // has been called, the controller will eventually delete itself and no member
  // functions should be called after that.
  void Accept();
  // Called by the Cleaner dialog when the dialog is closed via the cancel
  // button. Once |Cancel()| has been called, the controller will eventually
  // delete itself and no member functions should be called after that.
  void Cancel();
  // Called by the Cleaner dialog when the dialog is closed by some other means
  // than the cancel button (for example, by pressing Esc or clicking the 'x' on
  // the top of the dialog). After a call to |Dismiss()|, the controller will
  // eventually delete itself and no member functions should be called after
  // that.
  void Close();
  // Called when the details button is clicked, after which the dialog will
  // close. After a call to |DetailsButtonClicked()|, the controller will
  // eventually delete itself and no member functions should be called after
  // that.
  void DetailsButtonClicked();

 protected:
  ~ChromeCleanerDialogController();

 private:
  void OnInteractionDone();

  DISALLOW_COPY_AND_ASSIGN(ChromeCleanerDialogController);
};

}  // namespace safe_browsing

#endif  // CHROME_BROWSER_SAFE_BROWSING_CHROME_CLEANER_CHROME_CLEANER_DIALOG_CONTROLLER_WIN_H_
