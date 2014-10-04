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
  virtual void Show() override;
  virtual void Hide() override;
  virtual void UpdatesStarted() override;
  virtual void UpdatesFinished() override;
  virtual void UpdateAccountChooser() override;
  virtual void UpdateButtonStrip() override;
  virtual void UpdateOverlay() override;
  virtual void UpdateDetailArea() override;
  virtual void UpdateForErrors() override;
  virtual void UpdateNotificationArea() override;
  virtual void UpdateSection(DialogSection section) override;
  virtual void UpdateErrorBubble() override;
  virtual void FillSection(DialogSection section,
                           ServerFieldType originating_type) override;
  virtual void GetUserInput(DialogSection section,
                            FieldValueMap* output) override;
  virtual base::string16 GetCvc() override;
  virtual bool SaveDetailsLocally() override;
  virtual const content::NavigationController* ShowSignIn() override;
  virtual void HideSignIn() override;
  virtual void ModelChanged() override;
  virtual void OnSignInResize(const gfx::Size& pref_size) override;
  virtual void ValidateSection(DialogSection section) override;

  // ConstrainedWindowMacDelegate implementation:
  virtual void OnConstrainedWindowClosed(
      ConstrainedWindowMac* window) override;

  AutofillDialogViewDelegate* delegate() { return delegate_; }

  // Posts a close request on the current message loop.
  void PerformClose();

 private:
  friend class AutofillDialogViewTesterCocoa;

  // Closes the sheet and ends the modal loop. Triggers cleanup sequence.
  void CloseNow();

  scoped_ptr<ConstrainedWindowMac> constrained_window_;
  base::scoped_nsobject<AutofillDialogWindowController> sheet_delegate_;

  // The delegate |this| queries for logic and state.
  AutofillDialogViewDelegate* delegate_;

  // WeakPtrFactory for deferred close.
  base::WeakPtrFactory<AutofillDialogCocoa> close_weak_ptr_factory_;

};

}  // autofill

#endif  // CHROME_BROWSER_UI_COCOA_AUTOFILL_AUTOFILL_DIALOG_COCOA_H_
