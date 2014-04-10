// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_DIALOG_COCOA_H_
#define CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_DIALOG_COCOA_H_

#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/autofill/autofill_dialog_types.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/browser/ui/cocoa/constrained_window/constrained_window_mac.h"
#include "ui/gfx/size.h"

namespace content {
class NavigationController;
}

namespace autofill {
class AutofillDialogViewDelegate;
class AutofillDialogViewTesterCocoa;
}

@class AutofillDialogWindowController;

namespace autofill {

class AutofillDialogCocoa : public AutofillDialogView,
                            public ConstrainedWindowMacDelegate {
 public:
  explicit AutofillDialogCocoa(AutofillDialogViewDelegate* delegate);
  virtual ~AutofillDialogCocoa();

  // AutofillDialogView implementation:
  virtual void Show() OVERRIDE;
  virtual void Hide() OVERRIDE;
  virtual void UpdatesStarted() OVERRIDE;
  virtual void UpdatesFinished() OVERRIDE;
  virtual void UpdateAccountChooser() OVERRIDE;
  virtual void UpdateButtonStrip() OVERRIDE;
  virtual void UpdateOverlay() OVERRIDE;
  virtual void UpdateDetailArea() OVERRIDE;
  virtual void UpdateForErrors() OVERRIDE;
  virtual void UpdateNotificationArea() OVERRIDE;
  virtual void UpdateSection(DialogSection section) OVERRIDE;
  virtual void UpdateErrorBubble() OVERRIDE;
  virtual void FillSection(DialogSection section,
                           ServerFieldType originating_type) OVERRIDE;
  virtual void GetUserInput(DialogSection section,
                            FieldValueMap* output) OVERRIDE;
  virtual base::string16 GetCvc() OVERRIDE;
  virtual bool SaveDetailsLocally() OVERRIDE;
  virtual const content::NavigationController* ShowSignIn() OVERRIDE;
  virtual void HideSignIn() OVERRIDE;
  virtual void ModelChanged() OVERRIDE;
  virtual void OnSignInResize(const gfx::Size& pref_size) OVERRIDE;
  virtual void ValidateSection(DialogSection section) OVERRIDE;

  // ConstrainedWindowMacDelegate implementation:
  virtual void OnConstrainedWindowClosed(
      ConstrainedWindowMac* window) OVERRIDE;

  AutofillDialogViewDelegate* delegate() { return delegate_; }

  // Posts a close request on the current message loop.
  void PerformClose();

 private:
  friend class AutofillDialogViewTesterCocoa;

  // Closes the sheet and ends the modal loop. Triggers cleanup sequence.
  void CloseNow();

  scoped_ptr<ConstrainedWindowMac> constrained_window_;
  base::scoped_nsobject<AutofillDialogWindowController> sheet_delegate_;

  // WeakPtrFactory for deferred close.
  base::WeakPtrFactory<AutofillDialogCocoa> close_weak_ptr_factory_;

  // The delegate |this| queries for logic and state.
  AutofillDialogViewDelegate* delegate_;
};

}  // autofill

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_DIALOG_COCOA_H_
