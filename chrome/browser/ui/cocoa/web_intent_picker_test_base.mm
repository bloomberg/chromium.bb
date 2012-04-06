// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "web_intent_picker_test_base.h"

#include "chrome/browser/ui/cocoa/web_intent_picker_cocoa.h"
#import "chrome/browser/ui/cocoa/web_intent_sheet_controller.h"

MockIntentPickerDelegate::MockIntentPickerDelegate() {
}

MockIntentPickerDelegate::~MockIntentPickerDelegate() {
}

WebIntentPickerTestBase::WebIntentPickerTestBase() {
}

WebIntentPickerTestBase::~WebIntentPickerTestBase() {
}

void WebIntentPickerTestBase::CreateBubble(TabContentsWrapper* wrapper)
{
  picker_.reset(new WebIntentPickerCocoa(NULL, wrapper, &delegate_, &model_));

  controller_ =
     [[WebIntentPickerSheetController alloc] initWithPicker:picker_.get()];
  window_ = [controller_ window];
  [controller_ showWindow:nil];
}
