// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/debug/elf_reader_linux.h"

#include "base/strings/string_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace debug {

// The linker flag --build-id is passed only on official builds. Clang does not
// enable it by default and we do not have build id section in non-official
// builds.
#if defined(OFFICIAL_BUILD)
TEST(ElfReaderTest, ReadElfBuildId) {
  Optional<std::string> build_id = ReadElfBuildId();
  ASSERT_TRUE(build_id);
  const size_t kGuidBytes = 20;
  EXPECT_EQ(2 * kGuidBytes, build_id.value().size());
  for (char c : *build_id) {
    EXPECT_TRUE(IsHexDigit(c));
    EXPECT_FALSE(IsAsciiLower(c));
  }
}
#endif

}  // namespace debug
}  // namespace base
