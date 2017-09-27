// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/net/file_downloader.h"

#include "base/files/file_path.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/test/scoped_task_environment.h"
#include "base/threading/thread_task_runner_handle.h"
#include "content/public/test/test_utils.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/test_url_fetcher_factory.h"
#include "net/url_request/url_request_test_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

const char kURL[] = "https://www.url.com/path";
const char kFilename[] = "filename.ext";
const char kFileContents1[] = "file contents";
const char kFileContents2[] = "different contents";

class FileDownloaderTest : public testing::Test {
 public:
  FileDownloaderTest()
      : request_context_(new net::TestURLRequestContextGetter(
            base::ThreadTaskRunnerHandle::Get())),
            url_fetcher_factory_(nullptr) {}

  void SetUp() override {
    ASSERT_TRUE(dir_.CreateUniqueTempDir());
    path_ = dir_.GetPath().AppendASCII(kFilename);
    ASSERT_FALSE(base::PathExists(path_));
  }

  MOCK_METHOD1(OnDownloadFinished, void(FileDownloader::Result));

 protected:
  const base::FilePath& path() const { return path_; }

  void SetValidResponse() {
    url_fetcher_factory_.SetFakeResponse(
        GURL(kURL), kFileContents1, net::HTTP_OK,
        net::URLRequestStatus::SUCCESS);
  }

  void SetValidResponse2() {
    url_fetcher_factory_.SetFakeResponse(
        GURL(kURL), kFileContents2, net::HTTP_OK,
        net::URLRequestStatus::SUCCESS);
  }

  void SetFailedResponse() {
    url_fetcher_factory_.SetFakeResponse(
        GURL(kURL), std::string(), net::HTTP_NOT_FOUND,
        net::URLRequestStatus::SUCCESS);
  }

  void Download(bool overwrite, FileDownloader::Result expected_result) {
    FileDownloader downloader(
        GURL(kURL), path_, overwrite, request_context_.get(),
        base::Bind(&FileDownloaderTest::OnDownloadFinished,
                   base::Unretained(this)),
        TRAFFIC_ANNOTATION_FOR_TESTS);
    EXPECT_CALL(*this, OnDownloadFinished(expected_result));
    // Wait for the FileExists check to happen if necessary.
    content::RunAllTasksUntilIdle();
  }

 private:
  base::ScopedTempDir dir_;
  base::FilePath path_;

  base::test::ScopedTaskEnvironment scoped_task_environment_;
  scoped_refptr<net::TestURLRequestContextGetter> request_context_;
  net::FakeURLFetcherFactory url_fetcher_factory_;
};

TEST_F(FileDownloaderTest, Success) {
  SetValidResponse();
  Download(true, FileDownloader::DOWNLOADED);
  EXPECT_TRUE(base::PathExists(path()));
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(path(), &contents));
  EXPECT_EQ(std::string(kFileContents1), contents);
}

TEST_F(FileDownloaderTest, Failure) {
  SetFailedResponse();
  Download(true, FileDownloader::FAILED);
  EXPECT_FALSE(base::PathExists(path()));
}

TEST_F(FileDownloaderTest, Overwrite) {
  SetValidResponse();
  Download(true, FileDownloader::DOWNLOADED);
  ASSERT_TRUE(base::PathExists(path()));
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(path(), &contents));
  ASSERT_EQ(std::string(kFileContents1), contents);

  SetValidResponse2();
  Download(true, FileDownloader::DOWNLOADED);
  // The file should have been overwritten with the new contents.
  EXPECT_TRUE(base::PathExists(path()));
  ASSERT_TRUE(base::ReadFileToString(path(), &contents));
  EXPECT_EQ(std::string(kFileContents2), contents);
}

TEST_F(FileDownloaderTest, DontOverwrite) {
  SetValidResponse();
  Download(true, FileDownloader::DOWNLOADED);
  ASSERT_TRUE(base::PathExists(path()));
  std::string contents;
  ASSERT_TRUE(base::ReadFileToString(path(), &contents));
  EXPECT_EQ(std::string(kFileContents1), contents);

  SetValidResponse2();
  Download(false, FileDownloader::EXISTS);
  // The file should still have the old contents.
  EXPECT_TRUE(base::PathExists(path()));
  ASSERT_TRUE(base::ReadFileToString(path(), &contents));
  EXPECT_EQ(std::string(kFileContents1), contents);
}
