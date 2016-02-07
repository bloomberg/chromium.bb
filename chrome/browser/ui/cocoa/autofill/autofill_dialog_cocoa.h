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
#include "ui/gfx/geometry/size.h"

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
  ~AutofillDialogCocoa() override;

  // AutofillDialogView implementation:
  void Show() override;
  void Hide() override;
  void UpdatesStarted() override;
  void UpdatesFinished() override;
  void UpdateButtonStrip() override;
  void UpdateDetailArea() override;
  void UpdateForErrors() override;
  void UpdateNotificationArea() override;
  void UpdateSection(DialogSection section) override;
  void UpdateErrorBubble() override;
  void FillSection(DialogSection section,
                   ServerFieldType originating_type) override;
  void GetUserInput(DialogSection section, FieldValueMap* output) override;
  base::string16 GetCvc() override;
  bool SaveDetailsLocally() override;
  void ModelChanged() override;
  void ValidateSection(DialogSection section) override;

  // ConstrainedWindowMacDelegate implementation:
  void OnConstrainedWindowClosed(ConstrainedWindowMac* window) override;

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
