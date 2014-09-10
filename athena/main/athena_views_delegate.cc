// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/main/athena_views_delegate.h"

#include "athena/main/athena_frame_view.h"
#include "athena/screen/public/screen_manager.h"

namespace athena {

void AthenaViewsDelegate::OnBeforeWidgetInit(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
  params->context = athena::ScreenManager::Get()->GetContext();
}

views::NonClientFrameView* AthenaViewsDelegate::CreateDefaultNonClientFrameView(
    views::Widget* widget) {
  return new AthenaFrameView(widget);
}

}  // namespace athena
