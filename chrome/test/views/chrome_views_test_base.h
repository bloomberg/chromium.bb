// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_TEST_VIEWS_CHROME_VIEWS_TEST_BASE_H_
#define CHROME_TEST_VIEWS_CHROME_VIEWS_TEST_BASE_H_

#include <memory>

#include "base/macros.h"
#include "ui/views/test/views_test_base.h"

class ChromeLayoutProvider;

// A base class for Chrome views unit tests. Changes the dependencies when they
// need to be different than non-Chrome views.
class ChromeViewsTestBase : public views::ViewsTestBase {
 public:
  ChromeViewsTestBase();
  ~ChromeViewsTestBase() override;

  // views::ViewsTestBase:
  void SetUp() override;

 private:
  // Overrides the LayoutProvider created by ViewsTestBase (via
  // TestViewsDelegate).
  std::unique_ptr<ChromeLayoutProvider> provider_;

  DISALLOW_COPY_AND_ASSIGN(ChromeViewsTestBase);
};

#endif  // CHROME_TEST_VIEWS_CHROME_VIEWS_TEST_BASE_H_
