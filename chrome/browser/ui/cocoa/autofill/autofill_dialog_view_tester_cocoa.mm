// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_view_tester_cocoa.h"

#include "base/strings/sys_string_conversions.h"
#include "chrome/browser/ui/cocoa/autofill/autofill_dialog_cocoa.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_dialog_window_controller.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_main_container.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_section_container.h"
#import "chrome/browser/ui/cocoa/autofill/autofill_sign_in_container.h"


// Mirrors the AutofillDialogViewTester API on the C++ side.
@interface AutofillDialogWindowController (AutofillDialogViewTesterCocoa)

- (void)setTextContents:(NSString*)text
                forType:(autofill::ServerFieldType)type;
- (void)setTextContents:(NSString*)text
 ofSuggestionForSection:(autofill::DialogSection)section;
- (void)activateFieldForType:(autofill::ServerFieldType)type;
- (content::WebContents*)getSignInWebContents;
- (BOOL)isShowingOverlay;
- (BOOL)isShowingSection:(autofill::DialogSection)section;

@end


@implementation AutofillDialogWindowController (AutofillDialogViewTesterCocoa)

- (void)setTextContents:(NSString*)text
                forType:(autofill::ServerFieldType)type {
  for (size_t i = autofill::SECTION_MIN; i <= autofill::SECTION_MAX; ++i) {
    autofill::DialogSection section = static_cast<autofill::DialogSection>(i);
    if (!dialog_->delegate()->SectionIsActive(section))
      continue;
    // TODO(groby): Need to find the section for an input directly - wasteful.
    [[mainContainer_ sectionForId:section] setFieldValue:text forType:type];
  }
}

- (void)setTextContents:(NSString*)text
 ofSuggestionForSection:(autofill::DialogSection)section {
  [[mainContainer_ sectionForId:section] setSuggestionFieldValue:text];
}

- (void)activateFieldForType:(autofill::ServerFieldType)type {
  for (size_t i = autofill::SECTION_MIN; i <= autofill::SECTION_MAX; ++i) {
    autofill::DialogSection section = static_cast<autofill::DialogSection>(i);
    if (!dialog_->delegate()->SectionIsActive(section))
      continue;
    [[mainContainer_ sectionForId:section] activateFieldForType:type];
  }
}

- (content::WebContents*)getSignInWebContents {
  return [signInContainer_ webContents];
}

- (BOOL)isShowingOverlay {
  return ![[overlayController_ view] isHidden];
}

- (BOOL)isShowingSection:(autofill::DialogSection)section {
  return ![[[mainContainer_ sectionForId:section] view] isHidden];
}

@end

namespace autofill {

scoped_ptr<AutofillDialogViewTester> AutofillDialogViewTester::For(
    AutofillDialogView* dialog) {
  return scoped_ptr<AutofillDialogViewTester>(
      new AutofillDialogViewTesterCocoa(
          static_cast<AutofillDialogCocoa*>(dialog)));
}

AutofillDialogViewTesterCocoa::AutofillDialogViewTesterCocoa(
    AutofillDialogCocoa* dialog)
    : dialog_(dialog) {}

AutofillDialogViewTesterCocoa::~AutofillDialogViewTesterCocoa() {}

void AutofillDialogViewTesterCocoa::SubmitForTesting() {
  [controller() accept:nil];
}

void AutofillDialogViewTesterCocoa::CancelForTesting() {
  [controller() cancel:nil];
}

base::string16 AutofillDialogViewTesterCocoa::GetTextContentsOfInput(
    ServerFieldType type) {
  for (size_t i = SECTION_MIN; i <= SECTION_MAX; ++i) {
    DialogSection section = static_cast<DialogSection>(i);
    if (!dialog_->delegate()->SectionIsActive(section))
      continue;
    FieldValueMap contents;
    [controller() getInputs:&contents forSection:section];
    FieldValueMap::const_iterator it = contents.find(type);
    if (it != contents.end())
      return it->second;
  }

  NOTREACHED();
  return base::string16();
}

void AutofillDialogViewTesterCocoa::SetTextContentsOfInput(
    ServerFieldType type,
    const base::string16& contents) {
  [controller() setTextContents:base::SysUTF16ToNSString(contents)
                        forType:type];
}

void AutofillDialogViewTesterCocoa::SetTextContentsOfSuggestionInput(
    DialogSection section,
    const base::string16& text) {
  [controller() setTextContents:base::SysUTF16ToNSString(text)
         ofSuggestionForSection:section];
}

void AutofillDialogViewTesterCocoa::ActivateInput(ServerFieldType type) {
  [controller() activateFieldForType:type];
}

gfx::Size AutofillDialogViewTesterCocoa::GetSize() const {
  return gfx::Size(NSSizeToCGSize([[controller() window] frame].size));
}

content::WebContents* AutofillDialogViewTesterCocoa::GetSignInWebContents() {
  return [controller() getSignInWebContents];
}

bool AutofillDialogViewTesterCocoa::IsShowingOverlay() const {
  return [controller() isShowingOverlay];
}

bool AutofillDialogViewTesterCocoa::IsShowingSection(
    autofill::DialogSection section) const {
  return [controller() isShowingSection:section];
}

AutofillDialogWindowController*
    AutofillDialogViewTesterCocoa::controller() const {
  return dialog_->sheet_delegate_;
}

}  // namespace autofill
