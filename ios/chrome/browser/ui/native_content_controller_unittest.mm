// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/logging.h"
#include "base/mac/scoped_nsobject.h"
#import "ios/chrome/browser/ui/native_content_controller.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace {

TEST(NativeContentControllerTest, TestInitWithURL) {
  GURL url("http://foo.bar.com");
  base::scoped_nsobject<NativeContentController> controller(
      [[NativeContentController alloc] initWithURL:url]);
  EXPECT_EQ(url, controller.get().url);

  // There is no default view without a nib.
  EXPECT_EQ(nil, controller.get().view);
}

TEST(NativeContentControllerTest, TestInitWithEmptyNibNameAndURL) {
  GURL url("http://foo.bar.com");
  base::scoped_nsobject<NativeContentController> controller(
      [[NativeContentController alloc] initWithNibName:nil url:url]);
  EXPECT_EQ(url, controller.get().url);

  // There is no default view without a nib.
  EXPECT_EQ(nil, controller.get().view);
}

TEST(NativeContentControllerTest, TestInitWithNibAndURL) {
  GURL url("http://foo.bar.com");
  NSString* nibName = @"native_content_controller_test";
  base::scoped_nsobject<NativeContentController> controller(
      [[NativeContentController alloc] initWithNibName:nibName url:url]);
  EXPECT_EQ(url, controller.get().url);

  // Check that view is loaded from nib.
  EXPECT_NE(nil, controller.get().view);
  UIView* view = controller.get().view;
  EXPECT_EQ(1u, view.subviews.count);
  EXPECT_NE(nil, [view.subviews firstObject]);
  UILabel* label = [view.subviews firstObject];
  EXPECT_TRUE([label isKindOfClass:[UILabel class]]);
  EXPECT_TRUE([label.text isEqualToString:@"test pass"]);
}

}  // namespace
