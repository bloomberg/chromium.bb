// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_views_delegate.h"

views::Widget::InitParams::WindowOpacity
ChromeViewsDelegate::GetOpacityForInitParams(
    const views::Widget::InitParams& params) {
  return views::Widget::InitParams::TRANSLUCENT_WINDOW;
}
