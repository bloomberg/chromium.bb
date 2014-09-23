// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/main/athena_views_delegate.h"

#include "athena/main/athena_frame_view.h"
#include "athena/screen/public/screen_manager.h"
#include "ui/views/views_delegate.h"

namespace athena {

namespace {

class AthenaViewsDelegate: public views::ViewsDelegate {
 public:
  AthenaViewsDelegate() {
  }

  virtual ~AthenaViewsDelegate() {
  }

  virtual void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) OVERRIDE {
    params->context = athena::ScreenManager::Get()->GetContext();
  }

  virtual views::NonClientFrameView* CreateDefaultNonClientFrameView(
      views::Widget* widget) OVERRIDE {
    return new AthenaFrameView(widget);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(AthenaViewsDelegate);
};

}  // namespace

void CreateAthenaViewsDelegate() {
  views::ViewsDelegate::views_delegate = new AthenaViewsDelegate;
}

void ShutdownAthenaViewsDelegate() {
  CHECK(views::ViewsDelegate::views_delegate);
  delete views::ViewsDelegate::views_delegate;
  views::ViewsDelegate::views_delegate = NULL;
}

}  // namespace athena
