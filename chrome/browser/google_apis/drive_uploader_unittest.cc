// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/drive_uploader.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/google_apis/dummy_drive_service.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

const char kTestDummyId[] = "file:dummy_id";
const char kTestDocumentTitle[] = "Hello world";
const char kTestDrivePath[] = "drive/dummy.txt";
const char kTestInitiateUploadParentResourceId[] = "parent_resource_id";
const char kTestInitiateUploadResourceId[] = "resource_id";
const char kTestMimeType[] = "text/plain";
const char kTestUploadNewFileURL[] = "http://test/upload_location/new_file";
const char kTestUploadExistingFileURL[] =
    "http://test/upload_location/existing_file";
const int64 kUploadChunkSize = 512 * 1024;
const char kTestETag[] = "test_etag";

// Mock DriveService that verifies if the uploaded content matches the preset
// expectation.
class MockDriveServiceWithUploadExpectation : public DummyDriveService {
 public:
  // Sets up an expected upload content. InitiateUpload and ResumeUpload will
  // verify that the specified data is correctly uploaded.
  MockDriveServiceWithUploadExpectation(
      const base::FilePath& expected_upload_file,
      int64 expected_content_length)
     : expected_upload_file_(expected_upload_file),
       expected_content_length_(expected_content_length),
       received_bytes_(0),
       resume_upload_call_count_(0) {}

  int64 received_bytes() const { return received_bytes_; }
  void set_received_bytes(int64 received_bytes) {
    received_bytes_ = received_bytes;
  }

  int64 resume_upload_call_count() const { return resume_upload_call_count_; }

 private:
  // DriveServiceInterface overrides.
  // Handles a request for obtaining an upload location URL.
  virtual void InitiateUploadNewFile(
      const base::FilePath& drive_file_path,
      const std::string& content_type,
      int64 content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const InitiateUploadCallback& callback) OVERRIDE {
    EXPECT_EQ(kTestDocumentTitle, title);
    EXPECT_EQ(kTestMimeType, content_type);
    EXPECT_EQ(expected_content_length_, content_length);
    EXPECT_EQ(kTestInitiateUploadParentResourceId, parent_resource_id);

    // Calls back the upload URL for subsequent ResumeUpload operations.
    // InitiateUpload is an asynchronous function, so don't callback directly.
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback, HTTP_SUCCESS, GURL(kTestUploadNewFileURL)));
  }

  virtual void InitiateUploadExistingFile(
      const base::FilePath& drive_file_path,
      const std::string& content_type,
      int64 content_length,
      const std::string& resource_id,
      const std::string& etag,
      const InitiateUploadCallback& callback) OVERRIDE {
    EXPECT_EQ(kTestMimeType, content_type);
    EXPECT_EQ(expected_content_length_, content_length);
    EXPECT_EQ(kTestInitiateUploadResourceId, resource_id);

    if (!etag.empty() && etag != kTestETag) {
      MessageLoop::current()->PostTask(FROM_HERE,
          base::Bind(callback, HTTP_PRECONDITION, GURL()));
      return;
    }

    // Calls back the upload URL for subsequent ResumeUpload operations.
    // InitiateUpload is an asynchronous function, so don't callback directly.
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback, HTTP_SUCCESS, GURL(kTestUploadExistingFileURL)));
  }

  // Handles a request for uploading a chunk of bytes.
  virtual void ResumeUpload(
      const base::FilePath& drive_file_path,
      const GURL& upload_location,
      int64 start_position,
      int64 end_position,
      int64 content_length,
      const std::string& content_type,
      const base::FilePath& local_file_path,
      const UploadRangeCallback& callback,
      const ProgressCallback& progress_callback) OVERRIDE {
    // The upload range should start from the current first unreceived byte.
    EXPECT_EQ(received_bytes_, start_position);
    EXPECT_EQ(expected_upload_file_, local_file_path);

    // The upload data must be split into 512KB chunks.
    const int64 expected_chunk_end =
        std::min(received_bytes_ + kUploadChunkSize, expected_content_length_);
    EXPECT_EQ(expected_chunk_end, end_position);

    // The upload URL returned by InitiateUpload() must be used.
    EXPECT_TRUE(GURL(kTestUploadNewFileURL) == upload_location ||
                GURL(kTestUploadExistingFileURL) == upload_location);

    // Other parameters should be the exact values passed to DriveUploader.
    EXPECT_EQ(expected_content_length_, content_length);
    EXPECT_EQ(kTestMimeType, content_type);

    // Update the internal status of the current upload session.
    resume_upload_call_count_++;
    received_bytes_ = end_position;

    // Callback progress
    if (!progress_callback.is_null()) {
      // For the testing purpose, it always notifies the progress at the end of
      // each chunk uploading.
      int64 chunk_size = end_position - start_position;
      MessageLoop::current()->PostTask(FROM_HERE,
          base::Bind(progress_callback, chunk_size, chunk_size));
    }

    SendUploadRangeResponse(upload_location, callback);
  }

  // Handles a request to fetch the current upload status.
  virtual void GetUploadStatus(const base::FilePath& drive_file_path,
                               const GURL& upload_location,
                               int64 content_length,
                               const UploadRangeCallback& callback) OVERRIDE {
    EXPECT_EQ(expected_content_length_, content_length);
    // The upload URL returned by InitiateUpload() must be used.
    EXPECT_TRUE(GURL(kTestUploadNewFileURL) == upload_location ||
                GURL(kTestUploadExistingFileURL) == upload_location);

    SendUploadRangeResponse(upload_location, callback);
  }

  // Runs |callback| with the current upload status.
  void SendUploadRangeResponse(const GURL& upload_location,
                               const UploadRangeCallback& callback) {
    // Callback with response.
    UploadRangeResponse response;
    scoped_ptr<ResourceEntry> entry;
    if (received_bytes_ == expected_content_length_) {
      GDataErrorCode response_code =
          upload_location == GURL(kTestUploadNewFileURL) ?
          HTTP_CREATED : HTTP_SUCCESS;
      response = UploadRangeResponse(response_code, -1, -1);

      base::DictionaryValue dict;
      dict.Set("id.$t", new base::StringValue(kTestDummyId));
      entry = ResourceEntry::CreateFrom(dict);
    } else {
      response = UploadRangeResponse(
          HTTP_RESUME_INCOMPLETE, 0, received_bytes_);
    }
    // ResumeUpload is an asynchronous function, so don't callback directly.
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback, response, base::Passed(&entry)));
  }

  const base::FilePath expected_upload_file_;
  const int64 expected_content_length_;
  int64 received_bytes_;
  int64 resume_upload_call_count_;
};

// Mock DriveService that returns a failure at InitiateUpload().
class MockDriveServiceNoConnectionAtInitiate : public DummyDriveService {
  // Returns error.
  virtual void InitiateUploadNewFile(
      const base::FilePath& drive_file_path,
      const std::string& content_type,
      int64 content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const InitiateUploadCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback, GDATA_NO_CONNECTION, GURL()));
  }

  virtual void InitiateUploadExistingFile(
      const base::FilePath& drive_file_path,
      const std::string& content_type,
      int64 content_length,
      const std::string& resource_id,
      const std::string& etag,
      const InitiateUploadCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback, GDATA_NO_CONNECTION, GURL()));
  }

  // Should not be used.
  virtual void ResumeUpload(
      const base::FilePath& drive_file_path,
      const GURL& upload_url,
      int64 start_position,
      int64 end_position,
      int64 content_length,
      const std::string& content_type,
      const base::FilePath& local_file_path,
      const UploadRangeCallback& callback,
      const ProgressCallback& progress_callback) OVERRIDE {
    NOTREACHED();
  }
};

// Mock DriveService that returns a failure at ResumeUpload().
class MockDriveServiceNoConnectionAtResume : public DummyDriveService {
  // Succeeds and returns an upload location URL.
  virtual void InitiateUploadNewFile(
      const base::FilePath& drive_file_path,
      const std::string& content_type,
      int64 content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const InitiateUploadCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback, HTTP_SUCCESS, GURL(kTestUploadNewFileURL)));
  }

  virtual void InitiateUploadExistingFile(
      const base::FilePath& drive_file_path,
      const std::string& content_type,
      int64 content_length,
      const std::string& resource_id,
      const std::string& etag,
      const InitiateUploadCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback, HTTP_SUCCESS, GURL(kTestUploadExistingFileURL)));
  }

  // Returns error.
  virtual void ResumeUpload(
      const base::FilePath& drive_file_path,
      const GURL& upload_url,
      int64 start_position,
      int64 end_position,
      int64 content_length,
      const std::string& content_type,
      const base::FilePath& local_file_path,
      const UploadRangeCallback& callback,
      const ProgressCallback& progress_callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback,
                   UploadRangeResponse(GDATA_NO_CONNECTION, -1, -1),
                   base::Passed(scoped_ptr<ResourceEntry>())));
  }
};

class DriveUploaderTest : public testing::Test {
 public:
  DriveUploaderTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

  virtual void TearDown() OVERRIDE {
    ASSERT_TRUE(temp_dir_.Delete());
  }

 protected:
  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  base::ScopedTempDir temp_dir_;
};

}  // namespace

TEST_F(DriveUploaderTest, UploadExisting0KB) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(test_util::CreateFileOfSpecifiedSize(
      temp_dir_.path(), 0, &local_path, &data));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  scoped_ptr<ResourceEntry> resource_entry;

  MockDriveServiceWithUploadExpectation mock_service(local_path, data.size());
  DriveUploader uploader(&mock_service);
  std::vector<test_util::ProgressInfo> upload_progress_values;
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId,
      base::FilePath::FromUTF8Unsafe(kTestDrivePath),
      local_path,
      kTestMimeType,
      std::string(),  // etag
      test_util::CreateCopyResultCallback(
          &error, &upload_location, &resource_entry),
      base::Bind(&test_util::AppendProgressCallbackResult,
                 &upload_progress_values));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(1, mock_service.resume_upload_call_count());
  EXPECT_EQ(0, mock_service.received_bytes());
  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_TRUE(upload_location.is_empty());
  ASSERT_TRUE(resource_entry);
  EXPECT_EQ(kTestDummyId, resource_entry->id());
  ASSERT_EQ(1U, upload_progress_values.size());
  EXPECT_EQ(test_util::ProgressInfo(0, 0), upload_progress_values[0]);
}

TEST_F(DriveUploaderTest, UploadExisting512KB) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(test_util::CreateFileOfSpecifiedSize(
      temp_dir_.path(), 512 * 1024, &local_path, &data));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  scoped_ptr<ResourceEntry> resource_entry;

  MockDriveServiceWithUploadExpectation mock_service(local_path, data.size());
  DriveUploader uploader(&mock_service);
  std::vector<test_util::ProgressInfo> upload_progress_values;
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId,
      base::FilePath::FromUTF8Unsafe(kTestDrivePath),
      local_path,
      kTestMimeType,
      std::string(),  // etag
      test_util::CreateCopyResultCallback(
          &error, &upload_location, &resource_entry),
      base::Bind(&test_util::AppendProgressCallbackResult,
                 &upload_progress_values));
  test_util::RunBlockingPoolTask();

  // 512KB upload should not be split into multiple chunks.
  EXPECT_EQ(1, mock_service.resume_upload_call_count());
  EXPECT_EQ(512 * 1024, mock_service.received_bytes());
  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_TRUE(upload_location.is_empty());
  ASSERT_TRUE(resource_entry);
  EXPECT_EQ(kTestDummyId, resource_entry->id());
  ASSERT_EQ(1U, upload_progress_values.size());
  EXPECT_EQ(test_util::ProgressInfo(512 * 1024, 512 * 1024),
            upload_progress_values[0]);
}

TEST_F(DriveUploaderTest, InitiateUploadFail) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(test_util::CreateFileOfSpecifiedSize(
      temp_dir_.path(), 512 * 1024, &local_path, &data));

  GDataErrorCode error = HTTP_SUCCESS;
  GURL upload_location;
  scoped_ptr<ResourceEntry> resource_entry;

  MockDriveServiceNoConnectionAtInitiate mock_service;
  DriveUploader uploader(&mock_service);
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId,
      base::FilePath::FromUTF8Unsafe(kTestDrivePath),
      local_path,
      kTestMimeType,
      std::string(),  // etag
      test_util::CreateCopyResultCallback(
          &error, &upload_location, &resource_entry),
      google_apis::ProgressCallback());
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_TRUE(upload_location.is_empty());
  EXPECT_FALSE(resource_entry);
}

TEST_F(DriveUploaderTest, InitiateUploadNoConflict) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(test_util::CreateFileOfSpecifiedSize(
      temp_dir_.path(), 512 * 1024, &local_path, &data));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  scoped_ptr<ResourceEntry> resource_entry;

  MockDriveServiceWithUploadExpectation mock_service(local_path, data.size());
  DriveUploader uploader(&mock_service);
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId,
      base::FilePath::FromUTF8Unsafe(kTestDrivePath),
      local_path,
      kTestMimeType,
      kTestETag,
      test_util::CreateCopyResultCallback(
          &error, &upload_location, &resource_entry),
      google_apis::ProgressCallback());
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_TRUE(upload_location.is_empty());
}

TEST_F(DriveUploaderTest, InitiateUploadConflict) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(test_util::CreateFileOfSpecifiedSize(
      temp_dir_.path(), 512 * 1024, &local_path, &data));
  const std::string kDestinationETag("destination_etag");

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  scoped_ptr<ResourceEntry> resource_entry;

  MockDriveServiceWithUploadExpectation mock_service(local_path, data.size());
  DriveUploader uploader(&mock_service);
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId,
      base::FilePath::FromUTF8Unsafe(kTestDrivePath),
      local_path,
      kTestMimeType,
      kDestinationETag,
      test_util::CreateCopyResultCallback(
          &error, &upload_location, &resource_entry),
      google_apis::ProgressCallback());
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(HTTP_CONFLICT, error);
  EXPECT_TRUE(upload_location.is_empty());
}

TEST_F(DriveUploaderTest, ResumeUploadFail) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(test_util::CreateFileOfSpecifiedSize(
      temp_dir_.path(), 512 * 1024, &local_path, &data));

  GDataErrorCode error = HTTP_SUCCESS;
  GURL upload_location;
  scoped_ptr<ResourceEntry> resource_entry;

  MockDriveServiceNoConnectionAtResume mock_service;
  DriveUploader uploader(&mock_service);
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId,
      base::FilePath::FromUTF8Unsafe(kTestDrivePath),
      local_path,
      kTestMimeType,
      std::string(),  // etag
      test_util::CreateCopyResultCallback(
          &error, &upload_location, &resource_entry),
      google_apis::ProgressCallback());
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_EQ(GURL(kTestUploadExistingFileURL), upload_location);
}

TEST_F(DriveUploaderTest, NonExistingSourceFile) {
  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  scoped_ptr<ResourceEntry> resource_entry;

  DriveUploader uploader(NULL);  // NULL, the service won't be used.
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId,
      base::FilePath::FromUTF8Unsafe(kTestDrivePath),
      temp_dir_.path().AppendASCII("_this_path_should_not_exist_"),
      kTestMimeType,
      std::string(),             // etag
      test_util::CreateCopyResultCallback(
          &error, &upload_location, &resource_entry),
      google_apis::ProgressCallback());
  test_util::RunBlockingPoolTask();

  // Should return failure without doing any attempt to connect to the server.
  EXPECT_EQ(HTTP_NOT_FOUND, error);
  EXPECT_TRUE(upload_location.is_empty());
}

TEST_F(DriveUploaderTest, ResumeUpload) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(test_util::CreateFileOfSpecifiedSize(
      temp_dir_.path(), 1024 * 1024, &local_path, &data));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  scoped_ptr<ResourceEntry> resource_entry;

  MockDriveServiceWithUploadExpectation mock_service(local_path, data.size());
  DriveUploader uploader(&mock_service);
  // Emulate the situation that the only first part is successfully uploaded,
  // but not the latter half.
  mock_service.set_received_bytes(512 * 1024);

  std::vector<test_util::ProgressInfo> upload_progress_values;
  uploader.ResumeUploadFile(
      GURL(kTestUploadExistingFileURL),
      base::FilePath::FromUTF8Unsafe(kTestDrivePath),
      local_path,
      kTestMimeType,
      test_util::CreateCopyResultCallback(
          &error, &upload_location, &resource_entry),
      base::Bind(&test_util::AppendProgressCallbackResult,
                 &upload_progress_values));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(1, mock_service.resume_upload_call_count());
  EXPECT_EQ(1024 * 1024, mock_service.received_bytes());
  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_TRUE(upload_location.is_empty());
  ASSERT_TRUE(resource_entry);
  EXPECT_EQ(kTestDummyId, resource_entry->id());
  ASSERT_EQ(1U, upload_progress_values.size());
  EXPECT_EQ(test_util::ProgressInfo(1024 * 1024, 1024 * 1024),
            upload_progress_values[0]);
}

}  // namespace google_apis
