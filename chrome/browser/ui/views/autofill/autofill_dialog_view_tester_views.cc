// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/autofill/autofill_dialog_view_tester_views.h"

#include "base/logging.h"
#include "chrome/browser/ui/views/autofill/autofill_dialog_views.h"
#include "chrome/browser/ui/views/autofill/expanding_textfield.h"
#include "ui/base/models/combobox_model.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/webview/webview.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"

namespace autofill {

scoped_ptr<AutofillDialogViewTester> AutofillDialogViewTester::For(
    AutofillDialogView* view) {
  return scoped_ptr<AutofillDialogViewTester>(new
      AutofillDialogViewTesterViews(static_cast<AutofillDialogViews*>(view)));
}

AutofillDialogViewTesterViews::AutofillDialogViewTesterViews(
    AutofillDialogViews* view)
    : view_(view) {}

AutofillDialogViewTesterViews::~AutofillDialogViewTesterViews() {}

void AutofillDialogViewTesterViews::SubmitForTesting() {
  view_->Accept();
}

void AutofillDialogViewTesterViews::CancelForTesting() {
  view_->GetDialogClientView()->CancelWindow();
}

base::string16 AutofillDialogViewTesterViews::GetTextContentsOfInput(
    ServerFieldType type) {
  ExpandingTextfield* textfield = view_->TextfieldForType(type);
  if (textfield)
    return textfield->GetText();

  views::Combobox* combobox = view_->ComboboxForType(type);
  if (combobox)
    return combobox->model()->GetItemAt(combobox->selected_index());

  NOTREACHED();
  return base::string16();
}

void AutofillDialogViewTesterViews::SetTextContentsOfInput(
    ServerFieldType type,
    const base::string16& contents) {
  ExpandingTextfield* textfield = view_->TextfieldForType(type);
  if (textfield) {
    textfield->SetText(contents);
    return;
  }

  views::Combobox* combobox = view_->ComboboxForType(type);
  if (combobox) {
    if (!combobox->SelectValue(contents))
      combobox->SetSelectedIndex(combobox->model()->GetDefaultIndex());
    return;
  }

  NOTREACHED();
}

void AutofillDialogViewTesterViews::SetTextContentsOfSuggestionInput(
    DialogSection section,
    const base::string16& text) {
  view_->GroupForSection(section)->suggested_info->textfield()->SetText(text);
}

void AutofillDialogViewTesterViews::ActivateInput(ServerFieldType type) {
  view_->InputEditedOrActivated(type, gfx::Rect(), false);
}

gfx::Size AutofillDialogViewTesterViews::GetSize() const {
  return view_->GetWidget() ? view_->GetWidget()->GetRootView()->size() :
                              gfx::Size();
}

content::WebContents* AutofillDialogViewTesterViews::GetSignInWebContents() {
  return view_->sign_in_web_view_->web_contents();
}

bool AutofillDialogViewTesterViews::IsShowingOverlay() const {
  return view_->overlay_view_->visible();
}

bool AutofillDialogViewTesterViews::IsShowingSection(DialogSection section)
    const {
  return view_->GroupForSection(section)->container->visible();
}

}  // namespace autofill
