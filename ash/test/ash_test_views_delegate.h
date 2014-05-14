// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_TEST_VIEWS_DELEGATE_H_
#define ASH_TEST_ASH_TEST_VIEWS_DELEGATE_H_

#include "ui/views/test/test_views_delegate.h"

namespace ash {
namespace test {

// Ash specific ViewsDelegate. In addition to creating a TestWebContents this
// parents widget with no parent/context to the shell. This is created by
// default AshTestHelper.
class AshTestViewsDelegate : public views::TestViewsDelegate {
 public:
  AshTestViewsDelegate();
  virtual ~AshTestViewsDelegate();

  // Overriden from TestViewsDelegate.
  virtual content::WebContents* CreateWebContents(
      content::BrowserContext* browser_context,
      content::SiteInstance* site_instance) OVERRIDE;
  virtual void OnBeforeWidgetInit(
      views::Widget::InitParams* params,
      views::internal::NativeWidgetDelegate* delegate) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(AshTestViewsDelegate);
};

}  // namespace test
}  // namespace ash

#endif  // ASH_TEST_ASH_TEST_VIEWS_DELEGATE_H_

