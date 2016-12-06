// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "ios/chrome/browser/native_app_launcher/native_app_infobar_delegate.h"

#include <memory>

#include "base/memory/ptr_util.h"
#import "ios/public/provider/chrome/browser/native_app_launcher/native_app_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

#if !defined(__has_feature) || !__has_feature(objc_arc)
#error "This file requires ARC support."
#endif

namespace {

class NativeAppInfoBarDelegateTest : public PlatformTest {};

TEST_F(NativeAppInfoBarDelegateTest, TestIdentifiers) {
  std::unique_ptr<NativeAppInfoBarDelegate> delegate;

  delegate = base::MakeUnique<NativeAppInfoBarDelegate>(
      nil, GURL(), NATIVE_APP_INSTALLER_CONTROLLER);
  EXPECT_EQ(infobars::InfoBarDelegate::InfoBarIdentifier::
                NATIVE_APP_INSTALLER_INFOBAR_DELEGATE,
            delegate->GetIdentifier());

  delegate = base::MakeUnique<NativeAppInfoBarDelegate>(
      nil, GURL(), NATIVE_APP_LAUNCHER_CONTROLLER);
  EXPECT_EQ(infobars::InfoBarDelegate::InfoBarIdentifier::
                NATIVE_APP_LAUNCHER_INFOBAR_DELEGATE,
            delegate->GetIdentifier());

  delegate = base::MakeUnique<NativeAppInfoBarDelegate>(
      nil, GURL(), NATIVE_APP_OPEN_POLICY_CONTROLLER);
  EXPECT_EQ(infobars::InfoBarDelegate::InfoBarIdentifier::
                NATIVE_APP_OPEN_POLICY_INFOBAR_DELEGATE,
            delegate->GetIdentifier());
}

}  // namespace
