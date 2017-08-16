// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string>
#include <vector>

#include "base/files/file.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ptr_util.h"
#include "base/run_loop.h"
#include "base/time/time.h"
#include "chrome/browser/chromeos/file_manager/path_util.h"
#include "chrome/browser/chromeos/fileapi/recent_context.h"
#include "chrome/browser/chromeos/fileapi/recent_download_source.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "storage/browser/fileapi/external_mount_points.h"
#include "storage/browser/fileapi/file_system_context.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/browser/test/test_file_system_context.h"
#include "storage/common/fileapi/file_system_mount_option.h"
#include "storage/common/fileapi/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {
namespace {

class RecentDownloadSourceTest : public testing::Test {
 public:
  RecentDownloadSourceTest() : origin_("https://example.com/") {}

  void SetUp() override {
    profile_ = base::MakeUnique<TestingProfile>();

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    file_system_context_ = content::CreateFileSystemContextForTesting(
        nullptr, temp_dir_.GetPath());

    RegisterFakeDownloadsFileSystem();

    source_ = base::MakeUnique<RecentDownloadSource>(profile_.get(),
                                                     3 /* max_num_files */);

    base_time_ = base::Time::Now();
  }

 protected:
  void RegisterFakeDownloadsFileSystem() const {
    storage::ExternalMountPoints* mount_points =
        storage::ExternalMountPoints::GetSystemInstance();
    std::string mount_point_name =
        file_manager::util::GetDownloadsMountPointName(profile_.get());

    mount_points->RevokeFileSystem(mount_point_name);
    ASSERT_TRUE(mount_points->RegisterFileSystem(
        mount_point_name, storage::kFileSystemTypeTest,
        storage::FileSystemMountOption(), base::FilePath()));
  }

  bool CreateEmptyFile(const std::string& filename, int delta_time_in_seconds) {
    base::File file(temp_dir_.GetPath().Append(filename),
                    base::File::FLAG_CREATE | base::File::FLAG_WRITE);
    if (!file.IsValid())
      return false;

    base::Time time =
        base_time_ + base::TimeDelta::FromSeconds(delta_time_in_seconds);
    return file.SetTimes(time, time);
  }

  content::TestBrowserThreadBundle thread_bundle_;
  const GURL origin_;
  std::unique_ptr<TestingProfile> profile_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<storage::FileSystemContext> file_system_context_;
  std::unique_ptr<RecentDownloadSource> source_;
  base::Time base_time_;
};

TEST_F(RecentDownloadSourceTest, GetRecentFiles) {
  ASSERT_TRUE(CreateEmptyFile("1.jpg", 1));  // Oldest
  ASSERT_TRUE(CreateEmptyFile("2.jpg", 2));
  ASSERT_TRUE(CreateEmptyFile("3.jpg", 3));
  ASSERT_TRUE(CreateEmptyFile("4.jpg", 4));  // Newest

  base::RunLoop run_loop;

  source_->GetRecentFiles(
      RecentContext(file_system_context_.get(), origin_),
      base::BindOnce(
          [](base::RunLoop* run_loop,
             std::vector<storage::FileSystemURL> files) {
            run_loop->Quit();
            ASSERT_EQ(3u, files.size());
            EXPECT_EQ("2.jpg", files[0].path().BaseName().value());
            EXPECT_EQ("3.jpg", files[1].path().BaseName().value());
            EXPECT_EQ("4.jpg", files[2].path().BaseName().value());
          },
          &run_loop));

  run_loop.Run();
}

}  // namespace
}  // namespace chromeos
