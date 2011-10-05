// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extension_test_util {

std::string MakeId(std::string seed) {
  std::string result;
  bool success = Extension::GenerateId(seed, &result);
  EXPECT_TRUE(success);
  EXPECT_FALSE(result.empty());
  EXPECT_TRUE(Extension::IdIsValid(result));
  return result;
}

}  // namespace extension_test_util
