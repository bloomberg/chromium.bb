// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/intents/web_intent_picker_factory_impl.h"

#include "chrome/browser/ui/intents/web_intent_picker.h"
#include "content/browser/tab_contents/tab_contents.h"

WebIntentPickerFactoryImpl::WebIntentPickerFactoryImpl()
    : picker_(NULL) {
}

WebIntentPickerFactoryImpl::~WebIntentPickerFactoryImpl() {
  Close();
}

WebIntentPicker* WebIntentPickerFactoryImpl::Create(
    gfx::NativeWindow parent,
    TabContentsWrapper* wrapper,
    WebIntentPickerDelegate* delegate) {
  // Only allow one picker per factory.
  DCHECK(picker_ == NULL);

  picker_ = WebIntentPicker::Create(parent, wrapper, delegate);
  return picker_;
}

void WebIntentPickerFactoryImpl::ClosePicker(WebIntentPicker* picker) {
  DCHECK(picker == picker_);
  Close();
}

void WebIntentPickerFactoryImpl::Close() {
  if (picker_) {
    picker_->Close();
    picker_ = NULL;
  }
}
