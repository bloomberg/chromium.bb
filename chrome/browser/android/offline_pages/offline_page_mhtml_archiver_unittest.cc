// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/offline_pages/offline_page_mhtml_archiver.h"

#include <stdint.h>

#include <memory>

#include "base/bind.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/strings/string16.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace offline_pages {

namespace {

const char kTestURL[] = "http://example.com/";
const base::FilePath::CharType kTestFilePath[] = FILE_PATH_LITERAL(
    "/archive_dir/offline_page.mhtml");
const int64_t kTestFileSize = 123456LL;
const base::string16 kTestTitle = base::UTF8ToUTF16("a title");

class TestMHTMLArchiver : public OfflinePageMHTMLArchiver {
 public:
  enum class TestScenario {
    SUCCESS,
    NOT_ABLE_TO_ARCHIVE,
    WEB_CONTENTS_MISSING,
    CONNECTION_SECURITY_ERROR,
  };

  TestMHTMLArchiver(const GURL& url, const TestScenario test_scenario);
  ~TestMHTMLArchiver() override;

 private:
  void GenerateMHTML(const base::FilePath& archives_dir,
                     const CreateArchiveParams& create_archive_params) override;
  bool HasConnectionSecurityError() override;

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

void TestMHTMLArchiver::GenerateMHTML(
    const base::FilePath& archives_dir,
    const CreateArchiveParams& create_archive_params) {
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
                 base::Unretained(this), url_, base::FilePath(kTestFilePath),
                 kTestTitle, kTestFileSize));
}

bool TestMHTMLArchiver::HasConnectionSecurityError() {
  return test_scenario_ == TestScenario::CONNECTION_SECURITY_ERROR;
}

}  // namespace

class OfflinePageMHTMLArchiverTest : public testing::Test {
 public:
  OfflinePageMHTMLArchiverTest();
  ~OfflinePageMHTMLArchiverTest() override;

  // Creates an archiver for testing scenario and uses it to create an archive.
  std::unique_ptr<TestMHTMLArchiver> CreateArchive(
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
  int64_t last_file_size() const { return last_file_size_; }

  const OfflinePageArchiver::CreateArchiveCallback callback() {
    return base::Bind(&OfflinePageMHTMLArchiverTest::OnCreateArchiveDone,
                      base::Unretained(this));
  }

 private:
  void OnCreateArchiveDone(OfflinePageArchiver* archiver,
                           OfflinePageArchiver::ArchiverResult result,
                           const GURL& url,
                           const base::FilePath& file_path,
                           const base::string16& title,
                           int64_t file_size);

  OfflinePageArchiver* last_archiver_;
  OfflinePageArchiver::ArchiverResult last_result_;
  GURL last_url_;
  base::FilePath last_file_path_;
  int64_t last_file_size_;

  scoped_refptr<base::TestSimpleTaskRunner> task_runner_;
  base::ThreadTaskRunnerHandle task_runner_handle_;

  DISALLOW_COPY_AND_ASSIGN(OfflinePageMHTMLArchiverTest);
};

OfflinePageMHTMLArchiverTest::OfflinePageMHTMLArchiverTest()
    : last_archiver_(nullptr),
      last_result_(OfflinePageArchiver::ArchiverResult::
                       ERROR_ARCHIVE_CREATION_FAILED),
      last_file_size_(0L),
      task_runner_(new base::TestSimpleTaskRunner),
      task_runner_handle_(task_runner_) {
}

OfflinePageMHTMLArchiverTest::~OfflinePageMHTMLArchiverTest() {
}

std::unique_ptr<TestMHTMLArchiver> OfflinePageMHTMLArchiverTest::CreateArchive(
    const GURL& url,
    TestMHTMLArchiver::TestScenario scenario) {
  std::unique_ptr<TestMHTMLArchiver> archiver(
      new TestMHTMLArchiver(url, scenario));
  archiver->CreateArchive(GetTestFilePath(),
                          OfflinePageArchiver::CreateArchiveParams(),
                          callback());
  PumpLoop();
  return archiver;
}

void OfflinePageMHTMLArchiverTest::OnCreateArchiveDone(
    OfflinePageArchiver* archiver,
    OfflinePageArchiver::ArchiverResult result,
    const GURL& url,
    const base::FilePath& file_path,
    const base::string16& title,
    int64_t file_size) {
  last_url_ = url;
  last_archiver_ = archiver;
  last_result_ = result;
  last_file_path_ = file_path;
  last_file_size_ = file_size;
}

void OfflinePageMHTMLArchiverTest::PumpLoop() {
  task_runner_->RunUntilIdle();
}

// Tests that creation of an archiver fails when web contents is missing.
TEST_F(OfflinePageMHTMLArchiverTest, WebContentsMissing) {
  GURL page_url = GURL(kTestURL);
  std::unique_ptr<TestMHTMLArchiver> archiver(CreateArchive(
      page_url, TestMHTMLArchiver::TestScenario::WEB_CONTENTS_MISSING));

  EXPECT_EQ(archiver.get(), last_archiver());
  EXPECT_EQ(OfflinePageArchiver::ArchiverResult::ERROR_CONTENT_UNAVAILABLE,
            last_result());
  EXPECT_EQ(base::FilePath(), last_file_path());
}

// Tests for archiver failing save an archive.
TEST_F(OfflinePageMHTMLArchiverTest, NotAbleToGenerateArchive) {
  GURL page_url = GURL(kTestURL);
  std::unique_ptr<TestMHTMLArchiver> archiver(CreateArchive(
      page_url, TestMHTMLArchiver::TestScenario::NOT_ABLE_TO_ARCHIVE));

  EXPECT_EQ(archiver.get(), last_archiver());
  EXPECT_EQ(OfflinePageArchiver::ArchiverResult::ERROR_ARCHIVE_CREATION_FAILED,
            last_result());
  EXPECT_EQ(base::FilePath(), last_file_path());
  EXPECT_EQ(0LL, last_file_size());
}

// Tests for archiver handling of non-secure connection.
TEST_F(OfflinePageMHTMLArchiverTest, ConnectionNotSecure) {
  GURL page_url = GURL(kTestURL);
  std::unique_ptr<TestMHTMLArchiver> archiver(CreateArchive(
      page_url, TestMHTMLArchiver::TestScenario::CONNECTION_SECURITY_ERROR));

  EXPECT_EQ(archiver.get(), last_archiver());
  EXPECT_EQ(OfflinePageArchiver::ArchiverResult::ERROR_SECURITY_CERTIFICATE,
            last_result());
  EXPECT_EQ(base::FilePath(), last_file_path());
  EXPECT_EQ(0LL, last_file_size());
}

// Tests for successful creation of the offline page archive.
TEST_F(OfflinePageMHTMLArchiverTest, SuccessfullyCreateOfflineArchive) {
  GURL page_url = GURL(kTestURL);
  std::unique_ptr<TestMHTMLArchiver> archiver(
      CreateArchive(page_url, TestMHTMLArchiver::TestScenario::SUCCESS));
  PumpLoop();

  EXPECT_EQ(archiver.get(), last_archiver());
  EXPECT_EQ(OfflinePageArchiver::ArchiverResult::SUCCESSFULLY_CREATED,
            last_result());
  EXPECT_EQ(GetTestFilePath(), last_file_path());
  EXPECT_EQ(kTestFileSize, last_file_size());
}

}  // namespace offline_pages
