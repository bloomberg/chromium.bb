// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/chrome_views_delegate.h"

#include "ash/shell.h"

views::Widget::InitParams::WindowOpacity
ChromeViewsDelegate::GetOpacityForInitParams(
    const views::Widget::InitParams& params) {
  return views::Widget::InitParams::TRANSLUCENT_WINDOW;
}

views::NativeWidget* ChromeViewsDelegate::CreateNativeWidget(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
  // When we are doing straight chromeos builds, we still need to handle the
  // toplevel window case.
  // There may be a few remaining widgets in Chrome OS that are not top level,
  // but have neither a context nor a parent. Provide a fallback context so
  // users don't crash. Developers will hit the DCHECK and should provide a
  // context.
  if (params->context)
    params->context = params->context->GetRootWindow();
  DCHECK(params->parent || params->context || !params->child)
      << "Please provide a parent or context for this widget.";
  if (!params->parent && !params->context)
    params->context = ash::Shell::GetPrimaryRootWindow();

  // By returning null Widget creates the default NativeWidget implementation,
  // which for chromeos is NativeWidgetAura.
  return nullptr;
}
