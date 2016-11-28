// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_helper.h"
#include "ash/test/ash_test_views_delegate.h"

#include "ash/shell.h"

namespace ash {
namespace test {

AshTestViewsDelegate::AshTestViewsDelegate() {}

AshTestViewsDelegate::~AshTestViewsDelegate() {}

void AshTestViewsDelegate::OnBeforeWidgetInit(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
  TestViewsDelegate::OnBeforeWidgetInit(params, delegate);

  if (!params->parent && !params->context && ash::Shell::HasInstance()) {
    // If the window has neither a parent nor a context add to the root.
    params->parent = ash::Shell::GetInstance()->GetPrimaryRootWindow();
  }
}

void AshTestViewsDelegate::NotifyAccessibilityEvent(views::View* view,
                                                    ui::AXEvent event_type) {
  TestViewsDelegate::NotifyAccessibilityEvent(view, event_type);

  if (test_accessibility_event_delegate_) {
    test_accessibility_event_delegate_->NotifyAccessibilityEvent(view,
                                                                 event_type);
  }
}

}  // namespace test
}  // namespace ash
