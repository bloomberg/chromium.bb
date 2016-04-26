// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/safe_browsing/file_type_policies.h"

#include <string.h>

#include <memory>

#include "base/files/file_path.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using testing::NiceMock;

namespace safe_browsing {

class MockFileTypePolicies : public FileTypePolicies {
 public:
  MockFileTypePolicies() {}
  virtual ~MockFileTypePolicies() {}

  MOCK_METHOD2(RecordUpdateMetrics, void(UpdateResult, const std::string&));

 private:
  DISALLOW_COPY_AND_ASSIGN(MockFileTypePolicies);
};

class FileTypePoliciesTest : public testing::Test {
 protected:
  FileTypePoliciesTest() {}
  ~FileTypePoliciesTest() {}

 protected:
  NiceMock<MockFileTypePolicies> policies_;
};

TEST_F(FileTypePoliciesTest, UnpackResourceBundle) {
  EXPECT_CALL(policies_,
              RecordUpdateMetrics(FileTypePolicies::UpdateResult::SUCCESS,
                                  "ResourceBundle"));
  policies_.PopulateFromResourceBundle();

  // Look up a well known type to ensure it's present.
  base::FilePath exe_file(FILE_PATH_LITERAL("a/foo.exe"));
  DownloadFileType file_type = policies_.PolicyForFile(exe_file);
  EXPECT_EQ("exe", file_type.extension());
  EXPECT_EQ(0l, file_type.uma_value());
  EXPECT_EQ(false, file_type.is_archive());
  EXPECT_EQ(DownloadFileType::ALLOW_ON_USER_GESTURE,
            file_type.platform_settings(0).danger_level());
  EXPECT_EQ(DownloadFileType::DISALLOW_AUTO_OPEN,
            file_type.platform_settings(0).auto_open_hint());
  EXPECT_EQ(DownloadFileType::FULL_PING,
            file_type.platform_settings(0).ping_setting());

  // Check other accessors
  EXPECT_EQ(0l, policies_.UmaValueForFile(exe_file));
  EXPECT_EQ(false, policies_.IsFileAnArchive(exe_file));

  // Verify settings on default type.
  file_type = policies_.PolicyForFile(
      base::FilePath(FILE_PATH_LITERAL("a/foo.fooobar")));
  EXPECT_EQ("", file_type.extension());
  EXPECT_EQ(18l, file_type.uma_value());
  EXPECT_EQ(false, file_type.is_archive());
  EXPECT_EQ(DownloadFileType::ALLOW_ON_USER_GESTURE,
            file_type.platform_settings(0).danger_level());
  EXPECT_EQ(DownloadFileType::DISALLOW_AUTO_OPEN,
            file_type.platform_settings(0).auto_open_hint());
  EXPECT_EQ(DownloadFileType::SAMPLED_PING,
            file_type.platform_settings(0).ping_setting());
}

TEST_F(FileTypePoliciesTest, BadProto) {
  EXPECT_EQ(FileTypePolicies::UpdateResult::FAILED_EMPTY,
            policies_.PopulateFromBinaryPb(std::string()));

  EXPECT_EQ(FileTypePolicies::UpdateResult::FAILED_PROTO_PARSE,
            policies_.PopulateFromBinaryPb("foobar"));

  DownloadFileTypeConfig cfg;
  cfg.set_sampled_ping_probability(0.1f);
  EXPECT_EQ(FileTypePolicies::UpdateResult::FAILED_DEFAULT_SETTING_SET,
            policies_.PopulateFromBinaryPb(cfg.SerializeAsString()));

  cfg.mutable_default_file_type()->add_platform_settings();
  auto file_type = cfg.add_file_types();  // This is missing a platform_setting.
  EXPECT_EQ(FileTypePolicies::UpdateResult::FAILED_WRONG_SETTINGS_COUNT,
            policies_.PopulateFromBinaryPb(cfg.SerializeAsString()));

  file_type->add_platform_settings();
  EXPECT_EQ(FileTypePolicies::UpdateResult::SUCCESS,
            policies_.PopulateFromBinaryPb(cfg.SerializeAsString()));
}

TEST_F(FileTypePoliciesTest, BadUpdateFromExisting) {
  // Make a minimum viable config
  DownloadFileTypeConfig cfg;
  cfg.mutable_default_file_type()->add_platform_settings();
  cfg.add_file_types()->add_platform_settings();
  cfg.set_version_id(2);
  EXPECT_EQ(FileTypePolicies::UpdateResult::SUCCESS,
            policies_.PopulateFromBinaryPb(cfg.SerializeAsString()));

  // Can't update to the same version
  EXPECT_EQ(FileTypePolicies::UpdateResult::FAILED_VERSION_CHECK,
            policies_.PopulateFromBinaryPb(cfg.SerializeAsString()));

  // Can't go backward
  cfg.set_version_id(1);
  EXPECT_EQ(FileTypePolicies::UpdateResult::FAILED_VERSION_CHECK,
            policies_.PopulateFromBinaryPb(cfg.SerializeAsString()));
}
}  // namespace safe_browsing
