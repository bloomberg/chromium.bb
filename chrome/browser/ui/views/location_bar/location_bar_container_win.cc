// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/location_bar/location_bar_container.h"

#include "chrome/browser/ui/views/location_bar/location_bar_view.h"
#include "chrome/browser/ui/omnibox/omnibox_view.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/widget/widget.h"

void LocationBarContainer::OnFocus() {
  // Reenable when convert back to widget.
  /*
  // TODO(sky): verify if this is necessary.
  GetWidget()->NotifyAccessibilityEvent(
      this, ui::AccessibilityTypes::EVENT_FOCUS, false);
  */
  location_bar_view_->GetLocationEntry()->SetFocus();
}

void LocationBarContainer::PlatformInit() {
  view_parent_ = this;
  // TODO: this is right way to go about this on windows, but it leads to ugly
  // coordinate conversions. Need to straighten those out before reeabling.
  /*
  views::Widget* widget = new views::Widget();
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_CONTROL);
  params.can_activate = false;
  params.parent_widget = GetWidget();
  widget->Init(params);

  native_view_host_ = new views::NativeViewHost;
  AddChildView(native_view_host_);
  native_view_host_->Attach(widget->GetNativeView());
  widget->SetContentsView(new views::View);
  view_parent_ = widget->GetContentsView();
  view_parent_->SetLayoutManager(new views::FillLayout);

  widget->ShowInactive();  // Do not allow this widget to gain focus.
  */
}

// static
SkColor LocationBarContainer::GetBackgroundColor() {
  return color_utils::GetSysSkColor(COLOR_WINDOW);
}

void LocationBarContainer::StackAtTop() {
  view_parent_->GetWidget()->StackAtTop();
}
