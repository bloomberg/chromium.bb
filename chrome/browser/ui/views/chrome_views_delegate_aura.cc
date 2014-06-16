// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_views_delegate.h"

#include "chrome/browser/ui/ash/ash_util.h"

views::Widget::InitParams::WindowOpacity
ChromeViewsDelegate::GetOpacityForInitParams(
    const views::Widget::InitParams& params) {
  gfx::NativeView native_view = params.parent ? params.parent : params.context;
  if (native_view && chrome::IsNativeViewInAsh(native_view))
    return views::Widget::InitParams::TRANSLUCENT_WINDOW;

  // We want translucent windows when either we are in ASH or we are
  // a top level window which is not of type TYPE_WINDOW.
  if (!params.child && params.type != views::Widget::InitParams::TYPE_WINDOW)
    return views::Widget::InitParams::TRANSLUCENT_WINDOW;

  return views::Widget::InitParams::OPAQUE_WINDOW;
}
