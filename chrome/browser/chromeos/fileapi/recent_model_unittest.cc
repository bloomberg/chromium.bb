// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <algorithm>
#include <string>
#include <utility>

#include "base/files/file_path.h"
#include "base/memory/ptr_util.h"
#include "base/test/histogram_tester.h"
#include "chrome/browser/chromeos/fileapi/recent_file.h"
#include "chrome/browser/chromeos/fileapi/recent_model.h"
#include "chrome/browser/chromeos/fileapi/recent_model_factory.h"
#include "chrome/browser/chromeos/fileapi/test/fake_recent_source.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "storage/browser/fileapi/file_system_url.h"
#include "storage/common/fileapi/file_system_types.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

RecentFile MakeRecentFile(const std::string& name,
                          const base::Time& last_modified) {
  storage::FileSystemURL url = storage::FileSystemURL::CreateForTest(
      GURL(),  // origin
      storage::kFileSystemTypeNativeLocal, base::FilePath(name));
  return RecentFile(url, last_modified);
}

}  // namespace

class RecentModelTest : public testing::Test {
 public:
  RecentModelTest() = default;

 protected:
  std::vector<std::unique_ptr<RecentSource>> BuildDefaultSources() {
    auto source1 = base::MakeUnique<FakeRecentSource>();
    source1->AddFile(MakeRecentFile("aaa.jpg", base::Time::FromJavaTime(1000)));
    source1->AddFile(MakeRecentFile("ccc.jpg", base::Time::FromJavaTime(3000)));

    auto source2 = base::MakeUnique<FakeRecentSource>();
    source2->AddFile(MakeRecentFile("bbb.jpg", base::Time::FromJavaTime(2000)));
    source2->AddFile(MakeRecentFile("ddd.jpg", base::Time::FromJavaTime(4000)));

    std::vector<std::unique_ptr<RecentSource>> sources;
    sources.emplace_back(std::move(source1));
    sources.emplace_back(std::move(source2));
    return sources;
  }

  std::vector<storage::FileSystemURL> BuildModelAndGetRecentFiles(
      std::vector<std::unique_ptr<RecentSource>> sources,
      size_t max_files,
      const base::Time& cutoff_time) {
    RecentModel* model = RecentModelFactory::SetForProfileAndUseForTest(
        &profile_, RecentModel::CreateForTest(std::move(sources)));

    model->SetMaxFilesForTest(max_files);
    model->SetForcedCutoffTimeForTest(cutoff_time);

    std::vector<storage::FileSystemURL> urls;

    base::RunLoop run_loop;

    model->GetRecentFiles(
        nullptr /* file_system_context */, GURL() /* origin */,
        base::BindOnce(
            [](base::RunLoop* run_loop,
               std::vector<storage::FileSystemURL>* urls_out,
               const std::vector<storage::FileSystemURL>& urls) {
              *urls_out = urls;
              run_loop->Quit();
            },
            &run_loop, &urls));

    run_loop.Run();

    return urls;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  TestingProfile profile_;
};

TEST_F(RecentModelTest, GetRecentFiles) {
  std::vector<storage::FileSystemURL> urls =
      BuildModelAndGetRecentFiles(BuildDefaultSources(), 10, base::Time());

  ASSERT_EQ(4u, urls.size());
  EXPECT_EQ("ddd.jpg", urls[0].path().value());
  EXPECT_EQ("ccc.jpg", urls[1].path().value());
  EXPECT_EQ("bbb.jpg", urls[2].path().value());
  EXPECT_EQ("aaa.jpg", urls[3].path().value());
}

TEST_F(RecentModelTest, GetRecentFiles_MaxFiles) {
  std::vector<storage::FileSystemURL> urls =
      BuildModelAndGetRecentFiles(BuildDefaultSources(), 3, base::Time());

  ASSERT_EQ(3u, urls.size());
  EXPECT_EQ("ddd.jpg", urls[0].path().value());
  EXPECT_EQ("ccc.jpg", urls[1].path().value());
  EXPECT_EQ("bbb.jpg", urls[2].path().value());
}

TEST_F(RecentModelTest, GetRecentFiles_CutoffTime) {
  std::vector<storage::FileSystemURL> urls = BuildModelAndGetRecentFiles(
      BuildDefaultSources(), 10, base::Time::FromJavaTime(2500));

  ASSERT_EQ(2u, urls.size());
  EXPECT_EQ("ddd.jpg", urls[0].path().value());
  EXPECT_EQ("ccc.jpg", urls[1].path().value());
}

TEST_F(RecentModelTest, GetRecentFiles_UmaStats) {
  base::HistogramTester histogram_tester;

  BuildModelAndGetRecentFiles({}, 10, base::Time());

  histogram_tester.ExpectTotalCount(RecentModel::kLoadHistogramName, 1);
}

}  // namespace chromeos
