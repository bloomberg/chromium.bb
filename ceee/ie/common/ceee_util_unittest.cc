// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for the CEEE utilities.

#include "ceee/ie/common/ceee_util.h"

#include <ostream>  // NOLINT
#include <winerror.h>

#include "base/logging.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "ceee/testing/utils/mock_static.h"

namespace {

using testing::_;
using testing::Return;

TEST(CeeeUtilTest, IsIeCeeeRegisteredRealRegistry) {
  // This exercises the code but does not assert on the result,
  // it simply prints the result for human evaluation.
  //
  // It's hard to mock the HKCR registry so don't try for now;
  // further tests mock out the entire registry access.
  bool is_registered = ceee_util::IsIeCeeeRegistered();
  std::cout << "IsIeCeeeRegistered returned " << is_registered << std::endl;
}

MOCK_STATIC_CLASS_BEGIN(MockRegOpenKeyEx)
  MOCK_STATIC_INIT_BEGIN(MockRegOpenKeyEx)
    MOCK_STATIC_INIT(RegOpenKeyEx);
  MOCK_STATIC_INIT_END()

  MOCK_STATIC5(LSTATUS, APIENTRY, RegOpenKeyEx,
               HKEY, LPCWSTR, DWORD, REGSAM, PHKEY);
MOCK_STATIC_CLASS_END(MockRegOpenKeyEx)

TEST(CeeeUtilTest, IsIeCeeeRegisteredYes) {
  MockRegOpenKeyEx mock_reg;
  EXPECT_CALL(mock_reg, RegOpenKeyEx(_, _, _, _, _))
      .WillOnce(Return(ERROR_SUCCESS));
  ASSERT_TRUE(ceee_util::IsIeCeeeRegistered());
}

TEST(CeeeUtilTest, IsIeCeeeRegisteredNo) {
  MockRegOpenKeyEx mock_reg;
  EXPECT_CALL(mock_reg, RegOpenKeyEx(_, _, _, _, _))
      .WillOnce(Return(ERROR_FILE_NOT_FOUND));
  ASSERT_FALSE(ceee_util::IsIeCeeeRegistered());
}

}  // namespace
