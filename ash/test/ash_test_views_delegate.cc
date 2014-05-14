// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_test_views_delegate.h"

#include "ash/shell.h"
#include "content/public/test/web_contents_tester.h"

namespace ash {
namespace test {

AshTestViewsDelegate::AshTestViewsDelegate() {
}

AshTestViewsDelegate::~AshTestViewsDelegate() {
}

content::WebContents* AshTestViewsDelegate::CreateWebContents(
    content::BrowserContext* browser_context,
    content::SiteInstance* site_instance) {
  return content::WebContentsTester::CreateTestWebContents(browser_context,
                                                           site_instance);
}

void AshTestViewsDelegate::OnBeforeWidgetInit(
    views::Widget::InitParams* params,
    views::internal::NativeWidgetDelegate* delegate) {
  TestViewsDelegate::OnBeforeWidgetInit(params, delegate);

  if (!params->parent && !params->context && ash::Shell::HasInstance()) {
    // If the window has neither a parent nor a context add to the root.
    params->parent = ash::Shell::GetInstance()->GetPrimaryRootWindow();
  }
}

}  // namespace test
}  // namespace ash
