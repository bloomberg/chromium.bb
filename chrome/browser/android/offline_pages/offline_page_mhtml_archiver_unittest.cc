// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_mhtml_archiver.h"

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

const char kTestURL[] = "http://example.com/";
const base::FilePath::CharType kTestFilePath[] = FILE_PATH_LITERAL(
    "/archive_dir/offline_page.mhtml");
const int64 kTestFileSize = 123456LL;

class TestMHTMLArchiver : public OfflinePageMHTMLArchiver {
 public:
  enum class TestScenario {
    SUCCESS,
    NOT_ABLE_TO_ARCHIVE,
    WEB_CONTENTS_MISSING,
  };

  TestMHTMLArchiver(const GURL& url, const TestScenario test_scenario);
  ~TestMHTMLArchiver() override;

 private:
  void GenerateMHTML(const base::FilePath& archives_dir) override;

  const GURL url_;
  const TestScenario test_scenario_;

  DISALLOW_COPY_AND_ASSIGN(TestMHTMLArchiver);
};

TestMHTMLArchiver::TestMHTMLArchiver(const GURL& url,
                                     const TestScenario test_scenario)
    : url_(url),
      test_scenario_(test_scenario) {
}

TestMHTMLArchiver::~TestMHTMLArchiver() {
}

void TestMHTMLArchiver::GenerateMHTML(const base::FilePath& archives_dir) {
  if (test_scenario_ == TestScenario::WEB_CONTENTS_MISSING) {
    ReportFailure(ArchiverResult::ERROR_CONTENT_UNAVAILABLE);
    return;
  }

  if (test_scenario_ == TestScenario::NOT_ABLE_TO_ARCHIVE) {
    ReportFailure(ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED);
    return;
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&TestMHTMLArchiver::OnGenerateMHTMLDone,
                 base::Unretained(this),
                 url_,
                 base::FilePath(kTestFilePath),
                 kTestFileSize));
}

}  // namespace

class OfflinePageMHTMLArchiverTest : public testing::Test {
 public:
  OfflinePageMHTMLArchiverTest();
  ~OfflinePageMHTMLArchiverTest() override;

  // Creates an archiver for testing and specifies a scenario to be used.
  scoped_ptr<TestMHTMLArchiver> CreateArchiver(
      const GURL& url,
      TestMHTMLArchiver::TestScenario scenario);

  // Test tooling methods.
  void PumpLoop();

  base::FilePath GetTestFilePath() const {
    return base::FilePath(kTestFilePath);
  }

  const OfflinePageArchiver* last_archiver() const { return last_archiver_; }
  OfflinePageArchiver::ArchiverResult last_result() const {
    return last_result_;
  }
  const base::FilePath& last_file_path() const { return last_file_path_; }
  int64 last_file_size() const { return last_file_size_; }

  const OfflinePageArchiver::CreateArchiveCallback callback() {
    return base::Bind(&OfflinePageMHTMLArchiverTest::OnCreateArchiveDone,
                      base::Unretained(this));
  }

 private:
  void OnCreateArchiveDone(OfflinePageArchiver* archiver,
                           OfflinePageArchiver::ArchiverResult result,
                           const GURL& url,
                           const base::FilePath& file_path,
                           int64 file_size);

  OfflinePageArchiver* last_archiver_;
  OfflinePageArchiver::ArchiverResult last_result_;
  GURL last_url_;
  base::FilePath last_file_path_;
  int64 last_file_size_;

  base::MessageLoop message_loop_;
  scoped_ptr<base::RunLoop> run_loop_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageMHTMLArchiverTest);
};

OfflinePageMHTMLArchiverTest::OfflinePageMHTMLArchiverTest()
    : last_archiver_(nullptr),
      last_result_(OfflinePageArchiver::ArchiverResult::
                       ERROR_ARCHIVE_CREATION_FAILED),
      last_file_size_(0L) {
}

OfflinePageMHTMLArchiverTest::~OfflinePageMHTMLArchiverTest() {
}

scoped_ptr<TestMHTMLArchiver> OfflinePageMHTMLArchiverTest::CreateArchiver(
    const GURL& url,
    TestMHTMLArchiver::TestScenario scenario) {
  return scoped_ptr<TestMHTMLArchiver>(new TestMHTMLArchiver(url, scenario));
}

void OfflinePageMHTMLArchiverTest::OnCreateArchiveDone(
    OfflinePageArchiver* archiver,
    OfflinePageArchiver::ArchiverResult result,
    const GURL& url,
    const base::FilePath& file_path,
    int64 file_size) {
  if (run_loop_)
    run_loop_->Quit();
  last_url_ = url;
  last_archiver_ = archiver;
  last_result_ = result;
  last_file_path_ = file_path;
  last_file_size_ = file_size;
}

void OfflinePageMHTMLArchiverTest::PumpLoop() {
  run_loop_.reset(new base::RunLoop());
  run_loop_->Run();
}

// Tests that creation of an archiver fails when web contents is missing.
TEST_F(OfflinePageMHTMLArchiverTest, WebContentsMissing) {
  GURL page_url = GURL(kTestURL);
  scoped_ptr<TestMHTMLArchiver> archiver(
      CreateArchiver(page_url,
                     TestMHTMLArchiver::TestScenario::WEB_CONTENTS_MISSING));
  archiver->CreateArchive(GetTestFilePath(), callback());

  EXPECT_EQ(archiver.get(), last_archiver());
  EXPECT_EQ(OfflinePageArchiver::ArchiverResult::ERROR_CONTENT_UNAVAILABLE,
            last_result());
  EXPECT_EQ(base::FilePath(), last_file_path());
}

// Tests for successful creation of the offline page archive.
TEST_F(OfflinePageMHTMLArchiverTest, NotAbleToGenerateArchive) {
  GURL page_url = GURL(kTestURL);
  scoped_ptr<TestMHTMLArchiver> archiver(
      CreateArchiver(page_url,
                     TestMHTMLArchiver::TestScenario::NOT_ABLE_TO_ARCHIVE));
  archiver->CreateArchive(GetTestFilePath(), callback());

  EXPECT_EQ(archiver.get(), last_archiver());
  EXPECT_EQ(OfflinePageArchiver::ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED,
            last_result());
  EXPECT_EQ(base::FilePath(), last_file_path());
  EXPECT_EQ(0LL, last_file_size());
}

// Tests for successful creation of the offline page archive.
TEST_F(OfflinePageMHTMLArchiverTest, SuccessfullyCreateOfflineArchive) {
  GURL page_url = GURL(kTestURL);
  scoped_ptr<TestMHTMLArchiver> archiver(
      CreateArchiver(page_url,
                     TestMHTMLArchiver::TestScenario::SUCCESS));
  archiver->CreateArchive(GetTestFilePath(), callback());
  PumpLoop();

  EXPECT_EQ(archiver.get(), last_archiver());
  EXPECT_EQ(OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED,
            last_result());
  EXPECT_EQ(GetTestFilePath(), last_file_path());
  EXPECT_EQ(kTestFileSize, last_file_size());
}

TEST_F(OfflinePageMHTMLArchiverTest, GenerateFileName) {
  GURL url_1("http://news.google.com/page1");
  std::string title_1("Google News Page");
  base::FilePath expected_1(FILE_PATH_LITERAL(
      "news.google.com-Google_News_Page-mD2VzX6-h86e+Wl20CXh6VEkPXU=.mhtml"));
  base::FilePath actual_1(
      OfflinePageMHTMLArchiver::GenerateFileName(url_1, title_1));
  EXPECT_EQ(expected_1, actual_1);

  GURL url_2("https://en.m.wikipedia.org/Sample_page_about_stuff");
  std::string title_2("Some Wiki Page");
  base::FilePath expected_2(FILE_PATH_LITERAL(
      "en.m.wikipedia.org-Some_Wiki_Page-rEdSruS+14jgpnwJN9PGRUDpx9c=.mhtml"));
  base::FilePath actual_2(
      OfflinePageMHTMLArchiver::GenerateFileName(url_2, title_2));
  EXPECT_EQ(expected_2, actual_2);
}

}  // namespace offline_pages
