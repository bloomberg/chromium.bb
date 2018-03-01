// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/test/views/chrome_views_test_base.h"

#include "chrome/browser/ui/views/harmony/chrome_layout_provider.h"

ChromeViewsTestBase::ChromeViewsTestBase() {}

ChromeViewsTestBase::~ChromeViewsTestBase() {}

void ChromeViewsTestBase::SetUp() {
  views::ViewsTestBase::SetUp();
  provider_ = std::make_unique<ChromeLayoutProvider>();
}
