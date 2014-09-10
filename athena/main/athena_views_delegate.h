// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef ATHENA_MAIN_ATHENA_VIEWS_DELEGATE_H_
#define ATHENA_MAIN_ATHENA_VIEWS_DELEGATE_H_

#include "ui/views/views_delegate.h"

namespace athena {

class AthenaViewsDelegate : public views::ViewsDelegate {
 public:
  AthenaViewsDelegate() {}
  virtual ~AthenaViewsDelegate() {}

 private:
  // views::ViewsDelegate:
  virtual void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) OVERRIDE;
  virtual views::NonClientFrameView* CreateDefaultNonClientFrameView(
      views::Widget* widget) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(AthenaViewsDelegate);
};

}  // namespace athena

#endif  // ATHENA_MAIN_ATHENA_VIEWS_DELEGATE_H_
