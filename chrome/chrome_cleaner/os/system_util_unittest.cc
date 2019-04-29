// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/chrome_cleaner/os/system_util.h"

#include <memory>

#include "base/win/windows_version.h"
#include "chrome/chrome_cleaner/test/test_strings.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chrome_cleaner {

TEST(SystemUtilTests, GetMediumIntegrityToken) {
  base::win::ScopedHandle medium_integrity_token;
  ASSERT_TRUE(GetMediumIntegrityToken(&medium_integrity_token));

  // Get the list of privileges in the token.
  DWORD size = 0;
  ::GetTokenInformation(medium_integrity_token.Get(), TokenIntegrityLevel,
                        nullptr, 0, &size);
  std::unique_ptr<BYTE[]> mandatory_label_bytes(new BYTE[size]);
  TOKEN_MANDATORY_LABEL* mandatory_label =
      reinterpret_cast<TOKEN_MANDATORY_LABEL*>(mandatory_label_bytes.get());
  ASSERT_TRUE(::GetTokenInformation(medium_integrity_token.Get(),
                                    TokenIntegrityLevel, mandatory_label, size,
                                    &size));
  int32_t integrity_level = *GetSidSubAuthority(
      mandatory_label->Label.Sid,
      (DWORD)(UCHAR)(*GetSidSubAuthorityCount(mandatory_label->Label.Sid) - 1));
  EXPECT_GE(integrity_level, SECURITY_MANDATORY_MEDIUM_RID);
  EXPECT_LT(integrity_level, SECURITY_MANDATORY_HIGH_RID);
}

}  // namespace chrome_cleaner
