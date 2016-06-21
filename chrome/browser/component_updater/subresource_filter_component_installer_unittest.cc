// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/subresource_filter_component_installer.h"

#include <map>
#include <string>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace component_updater {

class SubresourceFilterComponentInstallerTest : public PlatformTest {
 public:
  SubresourceFilterComponentInstallerTest() {}
  void SetUp() override {
    PlatformTest::SetUp();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    traits_.reset(new SubresourceFilterComponentInstallerTraits());
  }

  void WriteSubresourceFilterToFile(
      const std::string subresource_filter_content,
      const base::FilePath& filename) {
    ASSERT_EQ(static_cast<int32_t>(subresource_filter_content.length()),
              base::WriteFile(filename, subresource_filter_content.c_str(),
                              subresource_filter_content.length()));
  }

  base::FilePath GetSubresourceFilterRulesDir() { return temp_dir_.path(); }

  void LoadSubresourceFilterRules(
      const base::DictionaryValue& manifest,
      const base::FilePath& subresource_filters_dir) {
    ASSERT_TRUE(traits_->VerifyInstallation(manifest, temp_dir_.path()));

    const base::Version v("1.0");
    traits_->LoadSubresourceFilterRulesFromDisk(subresource_filters_dir, v);
    // Drain the RunLoop created by the TestBrowserThreadBundle
    base::RunLoop().RunUntilIdle();
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;

  base::ScopedTempDir temp_dir_;
  std::unique_ptr<SubresourceFilterComponentInstallerTraits> traits_;

 private:
  DISALLOW_COPY_AND_ASSIGN(SubresourceFilterComponentInstallerTest);
};

TEST_F(SubresourceFilterComponentInstallerTest, LoadEmptyFile) {
  const base::DictionaryValue manifest;
  const base::FilePath subresource_filters_dir(GetSubresourceFilterRulesDir());

  const base::FilePath first_subresource_filter_file =
      subresource_filters_dir.Append(
          FILE_PATH_LITERAL("subresource_filter_rules.blob"));
  WriteSubresourceFilterToFile(std::string(), first_subresource_filter_file);

  LoadSubresourceFilterRules(manifest, subresource_filters_dir);
}

}  // namespace component_updater
