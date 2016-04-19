// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/component_updater/cld_component_installer.h"

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <utility>
#include <vector>

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "components/translate/content/browser/browser_cld_data_provider.h"
#include "components/translate/content/common/cld_data_source.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

using component_updater::CldComponentInstallerTraits;

namespace {

// This has to match what's in cld_component_installer.cc.
const base::FilePath::CharType kTestCldDataFileName[] =
    FILE_PATH_LITERAL("cld2_data.bin");

}  // namespace

namespace component_updater {

class CldComponentInstallerTest : public PlatformTest {
 public:
  CldComponentInstallerTest() {}
  void SetUp() override {
    PlatformTest::SetUp();
    translate::CldDataSource::DisableSanityChecksForTest();

    // ScopedTempDir automatically does a recursive delete on the entire
    // directory in its destructor, so no cleanup is required in TearDown.
    // Note that all files created by this test case are created within the
    // directory that is created here, so even though they are not explicitly
    // created *as temp files*, they will still get cleaned up automagically.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // The "latest CLD data file" is a static piece of information, and thus
    // for correctness we empty it before each test.
    CldComponentInstallerTraits::SetLatestCldDataFile(base::FilePath());
    base::FilePath path_now =
        CldComponentInstallerTraits::GetLatestCldDataFile();
    ASSERT_TRUE(path_now.empty());
  }

  void TearDown() override {
    // Restore sanity checks.
    translate::CldDataSource::EnableSanityChecksForTest();
  }

 protected:
  base::ScopedTempDir temp_dir_;
  CldComponentInstallerTraits traits_;

 private:
  DISALLOW_COPY_AND_ASSIGN(CldComponentInstallerTest);
};

TEST_F(CldComponentInstallerTest, SetLatestCldDataFile) {
  const base::FilePath expected(FILE_PATH_LITERAL("test/foo.test"));
  CldComponentInstallerTraits::SetLatestCldDataFile(expected);
  base::FilePath result = CldComponentInstallerTraits::GetLatestCldDataFile();
  ASSERT_EQ(expected, result);
}

TEST_F(CldComponentInstallerTest, VerifyInstallation) {
  // All files are created within a ScopedTempDir, which deletes all
  // children when its destructor is called (at the end of each test).
  const base::DictionaryValue manifest;
  ASSERT_FALSE(traits_.VerifyInstallation(manifest, temp_dir_.path()));
  const base::FilePath data_file_dir =
      temp_dir_.path()
          .Append(FILE_PATH_LITERAL("_platform_specific"))
          .Append(FILE_PATH_LITERAL("all"));
  ASSERT_TRUE(base::CreateDirectory(data_file_dir));
  const base::FilePath data_file = data_file_dir.Append(kTestCldDataFileName);
  const std::string test_data("fake cld2 data file content here :)");
  ASSERT_EQ(static_cast<int32_t>(test_data.length()),
            base::WriteFile(data_file, test_data.c_str(), test_data.length()));
  ASSERT_TRUE(traits_.VerifyInstallation(manifest, temp_dir_.path()));
}

TEST_F(CldComponentInstallerTest, OnCustomInstall) {
  const base::DictionaryValue manifest;
  const base::FilePath install_dir;
  // Sanity: shouldn't crash.
  ASSERT_TRUE(traits_.OnCustomInstall(manifest, install_dir));
}

TEST_F(CldComponentInstallerTest, GetInstalledPath) {
  const base::FilePath base_dir;
  const base::FilePath result =
      CldComponentInstallerTraits::GetInstalledPath(base_dir);
  ASSERT_TRUE(base::EndsWith(result.value(), kTestCldDataFileName,
                             base::CompareCase::SENSITIVE));
}

TEST_F(CldComponentInstallerTest, GetBaseDirectory) {
  const base::FilePath result = traits_.GetBaseDirectory();
  ASSERT_FALSE(result.empty());
}

TEST_F(CldComponentInstallerTest, GetHash) {
  std::vector<uint8_t> hash;
  traits_.GetHash(&hash);
  ASSERT_EQ(static_cast<size_t>(32), hash.size());
}

TEST_F(CldComponentInstallerTest, GetName) {
  ASSERT_FALSE(traits_.GetName().empty());
}

TEST_F(CldComponentInstallerTest, ComponentReady) {
  std::unique_ptr<base::DictionaryValue> manifest;
  const base::FilePath install_dir(FILE_PATH_LITERAL("/foo"));
  const base::Version version("1.2.3.4");
  traits_.ComponentReady(version, install_dir, std::move(manifest));
  base::FilePath result = CldComponentInstallerTraits::GetLatestCldDataFile();
  ASSERT_TRUE(base::StartsWith(result.AsUTF16Unsafe(),
                               install_dir.AsUTF16Unsafe(),
                               base::CompareCase::SENSITIVE));
  ASSERT_TRUE(base::EndsWith(result.value(), kTestCldDataFileName,
                             base::CompareCase::SENSITIVE));
}

}  // namespace component_updater
