// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/safe_browsing/incident_reporting/blacklist_load_analyzer.h"

#include <vector>

#include "base/files/file_path.h"
#include "base/path_service.h"
#include "base/scoped_native_library.h"
#include "base/strings/string_util.h"
#include "chrome_elf/blacklist/blacklist.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {
const wchar_t kTestDllName[] = L"blacklist_test_dll_1.dll";
}

namespace safe_browsing {

TEST(BlacklistLoadAnalyzer, TestBlacklistBypass) {
  base::FilePath current_dir;
  ASSERT_TRUE(PathService::Get(base::DIR_EXE, &current_dir));

  // Load test dll.
  base::ScopedNativeLibrary dll1(current_dir.Append(kTestDllName));

  // No blacklisted dlls should be found.
  std::vector<base::string16> module_names;
  EXPECT_TRUE(GetLoadedBlacklistedModules(&module_names));
  EXPECT_TRUE(module_names.empty());

  std::vector<base::string16>::const_iterator module_iter(module_names.begin());
  for (; module_iter != module_names.end(); ++module_iter) {
    LOG(ERROR) << "Found blacklisted module: " << *module_iter;
  }

  // Add test dll to blacklist
  blacklist::AddDllToBlacklist(kTestDllName);

  // Check that the test dll appears in list.
  module_names.clear();
  EXPECT_TRUE(GetLoadedBlacklistedModules(&module_names));
  ASSERT_EQ(1, module_names.size());
  EXPECT_STREQ(kTestDllName,
               base::StringToLowerASCII(
                   base::FilePath(module_names[0]).BaseName().value()).c_str());
}

}  // namespace safe_browsing
