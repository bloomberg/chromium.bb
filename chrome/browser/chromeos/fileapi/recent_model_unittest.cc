// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/chromeos/fileapi/recent_context.h"
#include "chrome/browser/chromeos/fileapi/recent_model.h"
#include "chrome/browser/chromeos/fileapi/recent_model_factory.h"
#include "chrome/browser/chromeos/fileapi/test/fake_recent_source.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/common/fileapi/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

using RecentFileList = RecentModel::RecentFileList;

class RecentModelTest : public testing::Test {
 public:
  RecentModelTest() = default;

 protected:
  RecentFileList BuildModelAndGetRecentFiles(
      std::vector<std::unique_ptr<RecentSource>> sources) {
    RecentModel* model = RecentModelFactory::SetForProfileAndUseForTest(
        &profile_, RecentModel::CreateForTest(std::move(sources)));

    RecentFileList files_out;

    base::RunLoop run_loop;

    model->GetRecentFiles(
        RecentContext(),
        base::BindOnce(
            [](base::RunLoop* run_loop, RecentModel::RecentFileList* files_out,
               const RecentModel::RecentFileList& files) {
              *files_out = files;
              run_loop->Quit();
            },
            &run_loop, &files_out));

    run_loop.Run();

    return files_out;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
};

storage::FileSystemURL MakeFileSystemURL(const std::string& name) {
  return storage::FileSystemURL::CreateForTest(
      GURL(),  // origin
      storage::kFileSystemTypeNativeLocal, base::FilePath(name));
}

TEST_F(RecentModelTest, GetRecentFiles) {
  auto source1 = base::MakeUnique<FakeRecentSource>();
  source1->AddFile(MakeFileSystemURL("aaa.jpg"));
  source1->AddFile(MakeFileSystemURL("ccc.jpg"));

  auto source2 = base::MakeUnique<FakeRecentSource>();
  source2->AddFile(MakeFileSystemURL("bbb.jpg"));
  source2->AddFile(MakeFileSystemURL("ddd.jpg"));

  std::vector<std::unique_ptr<RecentSource>> sources;
  sources.emplace_back(std::move(source1));
  sources.emplace_back(std::move(source2));

  RecentFileList files = BuildModelAndGetRecentFiles(std::move(sources));

  std::sort(files.begin(), files.end(), storage::FileSystemURL::Comparator());

  ASSERT_EQ(4u, files.size());
  EXPECT_EQ("aaa.jpg", files[0].path().value());
  EXPECT_EQ("bbb.jpg", files[1].path().value());
  EXPECT_EQ("ccc.jpg", files[2].path().value());
  EXPECT_EQ("ddd.jpg", files[3].path().value());
}

TEST_F(RecentModelTest, GetRecentFiles_UmaStats) {
  base::HistogramTester histogram_tester;

  BuildModelAndGetRecentFiles({});

  histogram_tester.ExpectTotalCount(RecentModel::kLoadHistogramName, 1);
}

}  // namespace chromeos
