// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intents/web_intent_constrained_dialog_factory.h"

#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "content/browser/tab_contents/constrained_window.h"
#include "content/browser/tab_contents/tab_contents.h"

WebIntentConstrainedDialogFactory::WebIntentConstrainedDialogFactory()
    : picker_(NULL) {
}

WebIntentConstrainedDialogFactory::~WebIntentConstrainedDialogFactory() {
  CloseDialog();
}

WebIntentPicker* WebIntentConstrainedDialogFactory::Create(
    TabContents* tab_contents,
    WebIntentPickerDelegate* delegate) {
  // Only allow one picker per factory.
  DCHECK(picker_ == NULL);

  picker_ = WebIntentPicker::Create(tab_contents, delegate);

  // TODO(binji) Remove this check when there are implementations of the picker
  // for windows and mac.
  if (picker_ == NULL)
    return NULL;

  picker_->Show();

  return picker_;
}

void WebIntentConstrainedDialogFactory::ClosePicker(WebIntentPicker* picker) {
  DCHECK(picker == picker_);
  CloseDialog();
}

void WebIntentConstrainedDialogFactory::CloseDialog() {
  if (picker_) {
    picker_->Close();
    picker_ = NULL;
  }
}
