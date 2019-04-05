// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/widget_finder.h"

#include "services/ws/window_service.h"
#include "ui/aura/window.h"
#include "ui/views/widget/widget.h"

namespace ash {

views::Widget* GetInternalWidgetForWindow(aura::Window* window) {
  return ws::WindowService::IsProxyWindow(window)
             ? nullptr
             : views::Widget::GetWidgetForNativeView(window);
}

}  // namespace ash
