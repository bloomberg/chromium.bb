// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_ASH_TEST_VIEWS_DELEGATE_WITH_PARENT_H_
#define CHROME_BROWSER_UI_ASH_TEST_VIEWS_DELEGATE_WITH_PARENT_H_

#include "ui/views/test/test_views_delegate.h"

// A views delegate which allows creating shell windows.
class TestViewsDelegateWithParent : public views::TestViewsDelegate {
 public:
  TestViewsDelegateWithParent();
  virtual ~TestViewsDelegateWithParent();

  // views::TestViewsDelegate overrides.
  virtual void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestViewsDelegateWithParent);
};

#endif  // CHROME_BROWSER_UI_ASH_TEST_VIEWS_DELEGATE_WITH_PARENT_H_
