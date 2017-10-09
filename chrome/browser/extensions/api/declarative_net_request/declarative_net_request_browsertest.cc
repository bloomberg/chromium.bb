// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/test/histogram_tester.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/extensions/extension_browsertest.h"
#include "extensions/browser/api/declarative_net_request/constants.h"
#include "extensions/browser/api/declarative_net_request/test_utils.h"
#include "extensions/common/api/declarative_net_request/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace declarative_net_request {
namespace {

enum class ExtensionLoadType {
  PACKED,
  UNPACKED,
};

constexpr char kJSONRulesFilename[] = "rules_file.json";
const base::FilePath::CharType kJSONRulesetFilepath[] =
    FILE_PATH_LITERAL("rules_file.json");

class DeclarativeNetRequestBrowserTest
    : public ExtensionBrowserTest,
      public ::testing::WithParamInterface<ExtensionLoadType> {
 public:
  DeclarativeNetRequestBrowserTest() {}

  // Loads an extension with the given declarative |rules|. Should be called
  // once per test.
  void LoadExtensionWithRules(const std::vector<TestRule>& rules) {
    base::ScopedAllowBlockingForTesting allow_blocking;
    base::HistogramTester tester;

    base::ScopedTempDir temp_dir;
    EXPECT_TRUE(temp_dir.CreateUniqueTempDir());

    base::FilePath extension_dir =
        temp_dir.GetPath().Append(FILE_PATH_LITERAL("test_extension"));
    EXPECT_TRUE(base::CreateDirectory(extension_dir));

    WriteManifestAndRuleset(extension_dir, kJSONRulesetFilepath,
                            kJSONRulesFilename, rules);

    const Extension* extension = nullptr;
    switch (GetParam()) {
      case ExtensionLoadType::PACKED:
        extension = InstallExtension(extension_dir, 1 /* expected_change */);
        break;
      case ExtensionLoadType::UNPACKED:
        extension = LoadExtension(extension_dir);
        break;
    }
    ASSERT_TRUE(extension);

    EXPECT_TRUE(HasValidIndexedRuleset(*extension, profile()));

    int expected_histogram_count = 1;
    tester.ExpectTotalCount(kIndexRulesTimeHistogram, expected_histogram_count);
    tester.ExpectTotalCount(kIndexAndPersistRulesTimeHistogram,
                            expected_histogram_count);
    tester.ExpectBucketCount(kManifestRulesCountHistogram, rules.size(),
                             expected_histogram_count);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(DeclarativeNetRequestBrowserTest);
};

// Test that an extension with declarative rules loads successfully.
IN_PROC_BROWSER_TEST_P(DeclarativeNetRequestBrowserTest, DNRExtension) {
  LoadExtensionWithRules({CreateGenericRule()});
}

INSTANTIATE_TEST_CASE_P(,
                        DeclarativeNetRequestBrowserTest,
                        ::testing::Values(ExtensionLoadType::PACKED,
                                          ExtensionLoadType::UNPACKED));

}  // namespace
}  // namespace declarative_net_request
}  // namespace extensions
