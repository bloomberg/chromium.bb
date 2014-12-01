// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/desktop_keyboard_capture.h"
#include "chrome/browser/ui/views/apps/keyboard_hook_handler.h"

DesktopKeyboardCapture::DesktopKeyboardCapture(views::Widget* widget)
    : widget_(widget), is_registered_(false) {
  widget_->AddObserver(this);

  if (widget_->IsActive())
    RegisterKeyboardHooks();
}

DesktopKeyboardCapture::~DesktopKeyboardCapture() {
  if (is_registered_)
    DeregisterKeyboardHooks();

  widget_->RemoveObserver(this);
}

void DesktopKeyboardCapture::RegisterKeyboardHooks() {
  KeyboardHookHandler::GetInstance()->Register(widget_);
  is_registered_ = true;
}

void DesktopKeyboardCapture::DeregisterKeyboardHooks() {
  KeyboardHookHandler::GetInstance()->Deregister(widget_);
  is_registered_ = false;
}

void DesktopKeyboardCapture::OnWidgetActivationChanged(views::Widget* widget,
                                                       bool active) {
  if (active) {
    RegisterKeyboardHooks();
  } else {
    DeregisterKeyboardHooks();
  }
}
