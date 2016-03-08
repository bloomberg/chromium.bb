// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_cocoa.h"

#include "base/bind.h"
#include "base/mac/scoped_nsobject.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view_delegate.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_details_container.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_dialog_window_controller.h"
#import "chrome/browser/ui/cocoa/constrained_window/constrained_window_custom_sheet.h"

namespace autofill {

// static
AutofillDialogView* AutofillDialogView::Create(
    AutofillDialogViewDelegate* delegate) {
  return new AutofillDialogCocoa(delegate);
}

AutofillDialogCocoa::AutofillDialogCocoa(AutofillDialogViewDelegate* delegate)
    : delegate_(delegate),
      close_weak_ptr_factory_(this) {
}

AutofillDialogCocoa::~AutofillDialogCocoa() {
}

void AutofillDialogCocoa::Show() {
  // This should only be called once.
  DCHECK(!sheet_delegate_.get());
  sheet_delegate_.reset([[AutofillDialogWindowController alloc]
       initWithWebContents:delegate_->GetWebContents()
                    dialog:this]);
  base::scoped_nsobject<CustomConstrainedWindowSheet> sheet(
      [[CustomConstrainedWindowSheet alloc]
          initWithCustomWindow:[sheet_delegate_ window]]);
  constrained_window_.reset(
      new ConstrainedWindowMac(this, delegate_->GetWebContents(), sheet));
  [sheet_delegate_ show];
}

void AutofillDialogCocoa::Hide() {
  [sheet_delegate_ hide];
}

void AutofillDialogCocoa::PerformClose() {
  if (!close_weak_ptr_factory_.HasWeakPtrs()) {
    base::MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&AutofillDialogCocoa::CloseNow,
                   close_weak_ptr_factory_.GetWeakPtr()));
  }
}

void AutofillDialogCocoa::CloseNow() {
  constrained_window_->CloseWebContentsModalDialog();
}

void AutofillDialogCocoa::UpdatesStarted() {
}

void AutofillDialogCocoa::UpdatesFinished() {
}

void AutofillDialogCocoa::UpdateButtonStrip() {
}

void AutofillDialogCocoa::UpdateDetailArea() {
}

void AutofillDialogCocoa::UpdateForErrors() {
  [sheet_delegate_ updateForErrors];
}

void AutofillDialogCocoa::UpdateNotificationArea() {
  [sheet_delegate_ updateNotificationArea];
}

void AutofillDialogCocoa::UpdateSection(DialogSection section) {
  [sheet_delegate_ updateSection:section];
}

void AutofillDialogCocoa::FillSection(DialogSection section,
                                      ServerFieldType originating_type) {
  [sheet_delegate_ fillSection:section forType:originating_type];
}

void AutofillDialogCocoa::GetUserInput(DialogSection section,
                                       FieldValueMap* output) {
  [sheet_delegate_ getInputs:output forSection:section];
}

base::string16 AutofillDialogCocoa::GetCvc() {
  return base::SysNSStringToUTF16([sheet_delegate_ getCvc]);
}

bool AutofillDialogCocoa::SaveDetailsLocally() {
  return [sheet_delegate_ saveDetailsLocally];
}

void AutofillDialogCocoa::ModelChanged() {
  [sheet_delegate_ modelChanged];
}

void AutofillDialogCocoa::UpdateErrorBubble() {
  [sheet_delegate_ updateErrorBubble];
}

void AutofillDialogCocoa::ValidateSection(DialogSection section) {
  [sheet_delegate_ validateSection:section];
}

void AutofillDialogCocoa::OnConstrainedWindowClosed(
    ConstrainedWindowMac* window) {
  constrained_window_.reset();
  // |this| belongs to |delegate_|, so no self-destruction here.
  delegate_->ViewClosed();
}

}  // autofill
