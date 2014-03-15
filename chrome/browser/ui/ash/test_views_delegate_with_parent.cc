// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/ash/test_views_delegate_with_parent.h"

#include "ash/shell.h"

TestViewsDelegateWithParent::TestViewsDelegateWithParent() {
}

TestViewsDelegateWithParent::~TestViewsDelegateWithParent() {
}

void TestViewsDelegateWithParent::OnBeforeWidgetInit(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
  if (!params->parent && !params->context) {
    // If the window has neither a parent nor a context we add the root window
    // as parent.
    params->parent = ash::Shell::GetInstance()->GetPrimaryRootWindow();
  }
}
