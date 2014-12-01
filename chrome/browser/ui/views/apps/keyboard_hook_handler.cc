// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/apps/keyboard_hook_handler.h"

#include "base/memory/singleton.h"

// TODO(sriramsr): Implement for other platforms.
// Empty implementation for other platforms.
void KeyboardHookHandler::Register(views::Widget* widget) {
}

void KeyboardHookHandler::Deregister(views::Widget* widget) {
}

// static
KeyboardHookHandler* KeyboardHookHandler::GetInstance() {
  return Singleton<KeyboardHookHandler,
                   DefaultSingletonTraits<KeyboardHookHandler>>::get();
}
