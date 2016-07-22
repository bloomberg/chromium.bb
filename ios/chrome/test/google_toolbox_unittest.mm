// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include "base/logging.h"
#include "testing/gtest_mac.h"
#import "third_party/google_toolbox_for_mac/src/Foundation/GTMNSDictionary+URLArguments.h"

// [NSDictionary gtm_dictionaryWithHttpArgumentsString] is used downstream.
// This test ensures that we keep compiling the file.
TEST(GoogleToolboxForMacTest, dictionaryWithHttpArgumentsString) {
  NSDictionary* dict = [NSDictionary gtm_dictionaryWithHttpArgumentsString:@""];
  EXPECT_EQ(0u, [dict count]);
}
