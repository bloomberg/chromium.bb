// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <vector>

#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/run_loop.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "base/version.h"
#include "chrome/browser/component_updater/cld_component_installer.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/platform_test.h"

namespace component_updater {

class CldComponentInstallerTest : public PlatformTest {
 public:
  virtual void SetUp() OVERRIDE {
    PlatformTest::SetUp();

    // ScopedTempDir automatically does a recursive delete on the entire
    // directory in its destructor, so no cleanup is required in TearDown.
    // Note that all files created by this test case are created within the
    // directory that is created here, so even though they are not explicitly
    // created *as temp files*, they will still get cleaned up automagically.
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    // The "latest CLD data file" is a static piece of information, and thus
    // for correctness we empty it before each test.
    traits.SetLatestCldDataFile(base::FilePath());
  }

  base::ScopedTempDir temp_dir_;
  component_updater::CldComponentInstallerTraits traits;
};

TEST_F(CldComponentInstallerTest, SetLatestCldDataFile) {
  ASSERT_TRUE(component_updater::GetLatestCldDataFile().empty());
  const base::FilePath expected(FILE_PATH_LITERAL("test/foo.test"));
  traits.SetLatestCldDataFile(expected);

  base::FilePath result = component_updater::GetLatestCldDataFile();
  ASSERT_EQ(expected, result);
}

TEST_F(CldComponentInstallerTest, VerifyInstallation) {
  // All files are created within a ScopedTempDir, which deletes all
  // children when its destructor is called (at the end of each test).
  ASSERT_FALSE(traits.VerifyInstallation(temp_dir_.path()));
  const base::FilePath data_file_dir =
      temp_dir_.path().Append(FILE_PATH_LITERAL("_platform_specific")).Append(
          FILE_PATH_LITERAL("all"));
  ASSERT_TRUE(base::CreateDirectory(data_file_dir));
  const base::FilePath data_file =
      data_file_dir.Append(chrome::kCLDDataFilename);
  const std::string test_data("fake cld2 data file content here :)");
  ASSERT_EQ(static_cast<int32>(test_data.length()),
            base::WriteFile(data_file, test_data.c_str(), test_data.length()));
  ASSERT_TRUE(traits.VerifyInstallation(temp_dir_.path()));
}

TEST_F(CldComponentInstallerTest, OnCustomInstall) {
  const base::DictionaryValue manifest;
  const base::FilePath install_dir;
  // Sanity: shouldn't crash.
  ASSERT_TRUE(traits.OnCustomInstall(manifest, install_dir));
}

TEST_F(CldComponentInstallerTest, GetInstalledPath) {
  const base::FilePath base_dir;
  const base::FilePath result =
      CldComponentInstallerTraits::GetInstalledPath(base_dir);
  ASSERT_TRUE(EndsWith(result.value(), chrome::kCLDDataFilename, true));
}

TEST_F(CldComponentInstallerTest, GetBaseDirectory) {
  const base::FilePath result = traits.GetBaseDirectory();
  ASSERT_FALSE(result.empty());
}

TEST_F(CldComponentInstallerTest, GetHash) {
  std::vector<uint8> hash;
  traits.GetHash(&hash);
  ASSERT_EQ(static_cast<size_t>(32), hash.size());
}

TEST_F(CldComponentInstallerTest, GetName) {
  ASSERT_FALSE(traits.GetName().empty());
}

TEST_F(CldComponentInstallerTest, ComponentReady) {
  scoped_ptr<base::DictionaryValue> manifest;
  const base::FilePath install_dir(FILE_PATH_LITERAL("/foo"));
  const base::Version version("1.2.3.4");
  traits.ComponentReady(version, install_dir, manifest.Pass());
  base::FilePath result = component_updater::GetLatestCldDataFile();
  ASSERT_TRUE(StartsWith(result.AsUTF16Unsafe(),
                         install_dir.AsUTF16Unsafe(),
                         true));
  ASSERT_TRUE(EndsWith(result.value(), chrome::kCLDDataFilename, true));
}

}  // namespace component_updater
