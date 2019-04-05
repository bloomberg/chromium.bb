// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_TEST_SPECIAL_USER_PROVIDER_H_
#define IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_TEST_SPECIAL_USER_PROVIDER_H_

#import "ios/public/provider/chrome/browser/user/special_user_provider.h"

#include "base/macros.h"

class TestSpecialUserProvider : public SpecialUserProvider {
 public:
  TestSpecialUserProvider();
  ~TestSpecialUserProvider() override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestSpecialUserProvider);
};

#endif  // IOS_PUBLIC_PROVIDER_CHROME_BROWSER_USER_TEST_SPECIAL_USER_PROVIDER_H_
