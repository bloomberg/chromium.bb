// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#include "base/memory/scoped_ptr.h"
#include "ios/web/navigation/navigation_item_impl.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/gtest_mac.h"
#include "testing/platform_test.h"

namespace web {
namespace {

class NavigationItemTest : public PlatformTest {
 protected:
  void SetUp() override { item_.reset(new NavigationItemImpl()); }

  scoped_ptr<NavigationItemImpl> item_;
};

// TODO(rohitrao): Add and adapt tests from NavigationEntryImpl.
TEST_F(NavigationItemTest, Dummy) {
  const GURL url("http://init.test");
  item_->SetURL(url);
  EXPECT_TRUE(item_->GetURL().is_valid());
}

}  // namespace
}  // namespace web
