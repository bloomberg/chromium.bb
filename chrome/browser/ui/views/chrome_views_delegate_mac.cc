// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_views_delegate.h"

#include "base/feature_list.h"
#include "chrome/common/chrome_features.h"

views::NativeWidget* ChromeViewsDelegate::CreateNativeWidget(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
  // By returning null Widget creates the default NativeWidget implementation.
  return nullptr;
}

bool ChromeViewsDelegate::ShouldMirrorArrowsInRTL() const {
  return base::FeatureList::IsEnabled(features::kMacRTL);
}

views::ViewsDelegate::ProcessMenuAcceleratorResult
ChromeViewsDelegate::ProcessAcceleratorWhileMenuShowing(
    const ui::Accelerator& accelerator) {
  using Result = views::ViewsDelegate::ProcessMenuAcceleratorResult;
  bool is_modified = accelerator.IsCtrlDown() || accelerator.IsAltDown() ||
                     accelerator.IsCmdDown();

  // Using an accelerator on Mac closes any open menu. Note that Mac behavior is
  // different between context menus (which block use of accelerators) and other
  // types of menus, which close when an accelerator is sent and do repost the
  // accelerator. In MacViews, this happens naturally because context menus are
  // (modal) Cocoa menus and other menus are Views menus, which will go through
  // this code path.
  return is_modified ? Result::CLOSE_MENU : Result::LEAVE_MENU_OPEN;
}
