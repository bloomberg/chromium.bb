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
    : close_weak_ptr_factory_(this),
      delegate_(delegate) {
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

void AutofillDialogCocoa::UpdateAccountChooser() {
  [sheet_delegate_ updateAccountChooser];
}

void AutofillDialogCocoa::UpdateButtonStrip() {
  [sheet_delegate_ updateButtonStrip];
}

void AutofillDialogCocoa::UpdateOverlay() {
  // TODO(estade): only update the overlay.
  UpdateButtonStrip();
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
                                      const DetailInput& originating_input) {
  [sheet_delegate_ fillSection:section forInput:originating_input];
}

void AutofillDialogCocoa::GetUserInput(DialogSection section,
                                       FieldValueMap* output) {
  [sheet_delegate_ getInputs:output forSection:section];
}

base::string16 AutofillDialogCocoa::GetCvc() {
  return base::SysNSStringToUTF16([sheet_delegate_ getCvc]);
}

bool AutofillDialogCocoa::HitTestInput(const DetailInput& input,
                                       const gfx::Point& screen_point) {
  // TODO(dbeam): implement.
  return false;
}

bool AutofillDialogCocoa::SaveDetailsLocally() {
  return [sheet_delegate_ saveDetailsLocally];
}

const content::NavigationController* AutofillDialogCocoa::ShowSignIn() {
  return [sheet_delegate_ showSignIn];
}

void AutofillDialogCocoa::HideSignIn() {
  [sheet_delegate_ hideSignIn];
}

void AutofillDialogCocoa::ModelChanged() {
  [sheet_delegate_ modelChanged];
}

void AutofillDialogCocoa::UpdateErrorBubble() {
  [sheet_delegate_ updateErrorBubble];
}

TestableAutofillDialogView* AutofillDialogCocoa::GetTestableView() {
  return this;
}

void AutofillDialogCocoa::OnSignInResize(const gfx::Size& pref_size) {
  [sheet_delegate_ onSignInResize:
      NSMakeSize(pref_size.width(), pref_size.height())];
}

void AutofillDialogCocoa::SubmitForTesting() {
  [sheet_delegate_ accept:nil];
}

void AutofillDialogCocoa::CancelForTesting() {
  [sheet_delegate_ cancel:nil];
}

base::string16 AutofillDialogCocoa::GetTextContentsOfInput(
    const DetailInput& input) {
  for (size_t i = SECTION_MIN; i <= SECTION_MAX; ++i) {
    DialogSection section = static_cast<DialogSection>(i);
    FieldValueMap contents;
    [sheet_delegate_ getInputs:&contents forSection:section];
    FieldValueMap::const_iterator it = contents.find(input.type);
    if (it != contents.end())
      return it->second;
  }

  NOTREACHED();
  return base::string16();
}

void AutofillDialogCocoa::SetTextContentsOfInput(
    const DetailInput& input,
    const base::string16& contents) {
  [sheet_delegate_ setTextContents:base::SysUTF16ToNSString(contents)
                          forInput:input];
}

void AutofillDialogCocoa::SetTextContentsOfSuggestionInput(
    DialogSection section,
    const base::string16& text) {
  [sheet_delegate_ setTextContents:base::SysUTF16ToNSString(text)
            ofSuggestionForSection:section];
}

void AutofillDialogCocoa::ActivateInput(const DetailInput& input) {
  [sheet_delegate_ activateFieldForInput:input];
}

gfx::Size AutofillDialogCocoa::GetSize() const {
  return gfx::Size(NSSizeToCGSize([[sheet_delegate_ window] frame].size));
}

content::WebContents* AutofillDialogCocoa::GetSignInWebContents() {
  return [sheet_delegate_ getSignInWebContents];
}


bool AutofillDialogCocoa::IsShowingOverlay() const {
  return [sheet_delegate_ isShowingOverlay];
}

void AutofillDialogCocoa::OnConstrainedWindowClosed(
    ConstrainedWindowMac* window) {
  constrained_window_.reset();
  // |this| belongs to |delegate_|, so no self-destruction here.
  delegate_->ViewClosed();
}

}  // autofill
