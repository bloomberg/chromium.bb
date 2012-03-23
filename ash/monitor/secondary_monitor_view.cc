// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/monitor/secondary_monitor_view.h"

#include "third_party/skia/include/core/SkColor.h"
#include "ui/aura/window.h"
#include "ui/views/view.h"
#include "ui/views/background.h"
#include "ui/views/widget/widget_delegate.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

const SkColor kBackground = SkColorSetRGB(0x33, 0x33, 0x33);

// A view to be displayed on secondary monitor.
class SecondaryMonitorView : public views::WidgetDelegateView {
 public:
  SecondaryMonitorView() {
    set_background(views::Background::CreateSolidBackground(kBackground));
  }
  ~SecondaryMonitorView() {
  }
};

}  // namespace

views::Widget* CreateSecondaryMonitorWidget(aura::Window* parent) {
  views::Widget* desktop_widget = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  SecondaryMonitorView* view = new SecondaryMonitorView();
  params.delegate = view;
  params.parent = parent;
  desktop_widget->Init(params);
  desktop_widget->SetContentsView(view);
  desktop_widget->Show();
  desktop_widget->GetNativeView()->SetName("SecondaryMonitor");
  return desktop_widget;
}

}  // namespace ash
