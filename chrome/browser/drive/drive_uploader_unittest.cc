// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/drive/drive_uploader.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/values.h"
#include "chrome/browser/drive/dummy_drive_service.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"

using google_apis::CancelCallback;
using google_apis::FileResource;
using google_apis::GDataErrorCode;
using google_apis::GDATA_NO_CONNECTION;
using google_apis::GDATA_OTHER_ERROR;
using google_apis::HTTP_CONFLICT;
using google_apis::HTTP_CREATED;
using google_apis::HTTP_NOT_FOUND;
using google_apis::HTTP_PRECONDITION;
using google_apis::HTTP_RESUME_INCOMPLETE;
using google_apis::HTTP_SUCCESS;
using google_apis::InitiateUploadCallback;
using google_apis::ProgressCallback;
using google_apis::UploadRangeResponse;
using google_apis::drive::UploadRangeCallback;
namespace test_util = google_apis::test_util;

namespace drive {

namespace {

const char kTestDummyMd5[] = "dummy_md5";
const char kTestDocumentTitle[] = "Hello world";
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
  virtual CancelCallback InitiateUploadNewFile(
      const std::string& content_type,
      int64 content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const InitiateUploadNewFileOptions& options,
      const InitiateUploadCallback& callback) OVERRIDE {
    EXPECT_EQ(kTestDocumentTitle, title);
    EXPECT_EQ(kTestMimeType, content_type);
    EXPECT_EQ(expected_content_length_, content_length);
    EXPECT_EQ(kTestInitiateUploadParentResourceId, parent_resource_id);

    // Calls back the upload URL for subsequent ResumeUpload requests.
    // InitiateUpload is an asynchronous function, so don't callback directly.
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback, HTTP_SUCCESS, GURL(kTestUploadNewFileURL)));
    return CancelCallback();
  }

  virtual CancelCallback InitiateUploadExistingFile(
      const std::string& content_type,
      int64 content_length,
      const std::string& resource_id,
      const InitiateUploadExistingFileOptions& options,
      const InitiateUploadCallback& callback) OVERRIDE {
    EXPECT_EQ(kTestMimeType, content_type);
    EXPECT_EQ(expected_content_length_, content_length);
    EXPECT_EQ(kTestInitiateUploadResourceId, resource_id);

    if (!options.etag.empty() && options.etag != kTestETag) {
      base::MessageLoop::current()->PostTask(FROM_HERE,
          base::Bind(callback, HTTP_PRECONDITION, GURL()));
      return CancelCallback();
    }

    // Calls back the upload URL for subsequent ResumeUpload requests.
    // InitiateUpload is an asynchronous function, so don't callback directly.
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback, HTTP_SUCCESS, GURL(kTestUploadExistingFileURL)));
    return CancelCallback();
  }

  // Handles a request for uploading a chunk of bytes.
  virtual CancelCallback ResumeUpload(
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
      base::MessageLoop::current()->PostTask(FROM_HERE,
          base::Bind(progress_callback, chunk_size, chunk_size));
    }

    SendUploadRangeResponse(upload_location, callback);
    return CancelCallback();
  }

  // Handles a request to fetch the current upload status.
  virtual CancelCallback GetUploadStatus(
      const GURL& upload_location,
      int64 content_length,
      const UploadRangeCallback& callback) OVERRIDE {
    EXPECT_EQ(expected_content_length_, content_length);
    // The upload URL returned by InitiateUpload() must be used.
    EXPECT_TRUE(GURL(kTestUploadNewFileURL) == upload_location ||
                GURL(kTestUploadExistingFileURL) == upload_location);

    SendUploadRangeResponse(upload_location, callback);
    return CancelCallback();
  }

  // Runs |callback| with the current upload status.
  void SendUploadRangeResponse(const GURL& upload_location,
                               const UploadRangeCallback& callback) {
    // Callback with response.
    UploadRangeResponse response;
    scoped_ptr<FileResource> entry;
    if (received_bytes_ == expected_content_length_) {
      GDataErrorCode response_code =
          upload_location == GURL(kTestUploadNewFileURL) ?
          HTTP_CREATED : HTTP_SUCCESS;
      response = UploadRangeResponse(response_code, -1, -1);

      entry.reset(new FileResource);
      entry->set_md5_checksum(kTestDummyMd5);
    } else {
      response = UploadRangeResponse(
          HTTP_RESUME_INCOMPLETE, 0, received_bytes_);
    }
    // ResumeUpload is an asynchronous function, so don't callback directly.
    base::MessageLoop::current()->PostTask(FROM_HERE,
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
  virtual CancelCallback InitiateUploadNewFile(
      const std::string& content_type,
      int64 content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const InitiateUploadNewFileOptions& options,
      const InitiateUploadCallback& callback) OVERRIDE {
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback, GDATA_NO_CONNECTION, GURL()));
    return CancelCallback();
  }

  virtual CancelCallback InitiateUploadExistingFile(
      const std::string& content_type,
      int64 content_length,
      const std::string& resource_id,
      const InitiateUploadExistingFileOptions& options,
      const InitiateUploadCallback& callback) OVERRIDE {
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback, GDATA_NO_CONNECTION, GURL()));
    return CancelCallback();
  }

  // Should not be used.
  virtual CancelCallback ResumeUpload(
      const GURL& upload_url,
      int64 start_position,
      int64 end_position,
      int64 content_length,
      const std::string& content_type,
      const base::FilePath& local_file_path,
      const UploadRangeCallback& callback,
      const ProgressCallback& progress_callback) OVERRIDE {
    NOTREACHED();
    return CancelCallback();
  }
};

// Mock DriveService that returns a failure at ResumeUpload().
class MockDriveServiceNoConnectionAtResume : public DummyDriveService {
  // Succeeds and returns an upload location URL.
  virtual CancelCallback InitiateUploadNewFile(
      const std::string& content_type,
      int64 content_length,
      const std::string& parent_resource_id,
      const std::string& title,
      const InitiateUploadNewFileOptions& options,
      const InitiateUploadCallback& callback) OVERRIDE {
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback, HTTP_SUCCESS, GURL(kTestUploadNewFileURL)));
    return CancelCallback();
  }

  virtual CancelCallback InitiateUploadExistingFile(
      const std::string& content_type,
      int64 content_length,
      const std::string& resource_id,
      const InitiateUploadExistingFileOptions& options,
      const InitiateUploadCallback& callback) OVERRIDE {
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback, HTTP_SUCCESS, GURL(kTestUploadExistingFileURL)));
    return CancelCallback();
  }

  // Returns error.
  virtual CancelCallback ResumeUpload(
      const GURL& upload_url,
      int64 start_position,
      int64 end_position,
      int64 content_length,
      const std::string& content_type,
      const base::FilePath& local_file_path,
      const UploadRangeCallback& callback,
      const ProgressCallback& progress_callback) OVERRIDE {
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback,
                   UploadRangeResponse(GDATA_NO_CONNECTION, -1, -1),
                   base::Passed(scoped_ptr<FileResource>())));
    return CancelCallback();
  }
};

// Mock DriveService that returns a failure at GetUploadStatus().
class MockDriveServiceNoConnectionAtGetUploadStatus : public DummyDriveService {
  // Returns error.
  virtual CancelCallback GetUploadStatus(
      const GURL& upload_url,
      int64 content_length,
      const UploadRangeCallback& callback) OVERRIDE {
    base::MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback,
                   UploadRangeResponse(GDATA_NO_CONNECTION, -1, -1),
                   base::Passed(scoped_ptr<FileResource>())));
    return CancelCallback();
  }
};

class DriveUploaderTest : public testing::Test {
 public:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
  }

 protected:
  content::TestBrowserThreadBundle thread_bundle_;
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
  scoped_ptr<FileResource> entry;

  MockDriveServiceWithUploadExpectation mock_service(local_path, data.size());
  DriveUploader uploader(&mock_service,
                         base::MessageLoopProxy::current().get());
  std::vector<test_util::ProgressInfo> upload_progress_values;
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId,
      local_path,
      kTestMimeType,
      DriveUploader::UploadExistingFileOptions(),
      test_util::CreateCopyResultCallback(
          &error, &upload_location, &entry),
      base::Bind(&test_util::AppendProgressCallbackResult,
                 &upload_progress_values));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, mock_service.resume_upload_call_count());
  EXPECT_EQ(0, mock_service.received_bytes());
  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_TRUE(upload_location.is_empty());
  ASSERT_TRUE(entry);
  EXPECT_EQ(kTestDummyMd5, entry->md5_checksum());
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
  scoped_ptr<FileResource> entry;

  MockDriveServiceWithUploadExpectation mock_service(local_path, data.size());
  DriveUploader uploader(&mock_service,
                         base::MessageLoopProxy::current().get());
  std::vector<test_util::ProgressInfo> upload_progress_values;
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId,
      local_path,
      kTestMimeType,
      DriveUploader::UploadExistingFileOptions(),
      test_util::CreateCopyResultCallback(
          &error, &upload_location, &entry),
      base::Bind(&test_util::AppendProgressCallbackResult,
                 &upload_progress_values));
  base::RunLoop().RunUntilIdle();

  // 512KB upload should not be split into multiple chunks.
  EXPECT_EQ(1, mock_service.resume_upload_call_count());
  EXPECT_EQ(512 * 1024, mock_service.received_bytes());
  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_TRUE(upload_location.is_empty());
  ASSERT_TRUE(entry);
  EXPECT_EQ(kTestDummyMd5, entry->md5_checksum());
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
  scoped_ptr<FileResource> entry;

  MockDriveServiceNoConnectionAtInitiate mock_service;
  DriveUploader uploader(&mock_service,
                         base::MessageLoopProxy::current().get());
  uploader.UploadExistingFile(kTestInitiateUploadResourceId,
                              local_path,
                              kTestMimeType,
                              DriveUploader::UploadExistingFileOptions(),
                              test_util::CreateCopyResultCallback(
                                  &error, &upload_location, &entry),
                              google_apis::ProgressCallback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_TRUE(upload_location.is_empty());
  EXPECT_FALSE(entry);
}

TEST_F(DriveUploaderTest, InitiateUploadNoConflict) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(test_util::CreateFileOfSpecifiedSize(
      temp_dir_.path(), 512 * 1024, &local_path, &data));

  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  scoped_ptr<FileResource> entry;

  MockDriveServiceWithUploadExpectation mock_service(local_path, data.size());
  DriveUploader uploader(&mock_service,
                         base::MessageLoopProxy::current().get());
  DriveUploader::UploadExistingFileOptions options;
  options.etag = kTestETag;
  uploader.UploadExistingFile(kTestInitiateUploadResourceId,
                              local_path,
                              kTestMimeType,
                              options,
                              test_util::CreateCopyResultCallback(
                                  &error, &upload_location, &entry),
                              google_apis::ProgressCallback());
  base::RunLoop().RunUntilIdle();

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
  scoped_ptr<FileResource> entry;

  MockDriveServiceWithUploadExpectation mock_service(local_path, data.size());
  DriveUploader uploader(&mock_service,
                         base::MessageLoopProxy::current().get());
  DriveUploader::UploadExistingFileOptions options;
  options.etag = kDestinationETag;
  uploader.UploadExistingFile(kTestInitiateUploadResourceId,
                              local_path,
                              kTestMimeType,
                              options,
                              test_util::CreateCopyResultCallback(
                                  &error, &upload_location, &entry),
                              google_apis::ProgressCallback());
  base::RunLoop().RunUntilIdle();

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
  scoped_ptr<FileResource> entry;

  MockDriveServiceNoConnectionAtResume mock_service;
  DriveUploader uploader(&mock_service,
                         base::MessageLoopProxy::current().get());
  uploader.UploadExistingFile(kTestInitiateUploadResourceId,
                              local_path,
                              kTestMimeType,
                              DriveUploader::UploadExistingFileOptions(),
                              test_util::CreateCopyResultCallback(
                                  &error, &upload_location, &entry),
                              google_apis::ProgressCallback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_EQ(GURL(kTestUploadExistingFileURL), upload_location);
}

TEST_F(DriveUploaderTest, GetUploadStatusFail) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(test_util::CreateFileOfSpecifiedSize(
      temp_dir_.path(), 512 * 1024, &local_path, &data));

  GDataErrorCode error = HTTP_SUCCESS;
  GURL upload_location;
  scoped_ptr<FileResource> entry;

  MockDriveServiceNoConnectionAtGetUploadStatus mock_service;
  DriveUploader uploader(&mock_service,
                         base::MessageLoopProxy::current().get());
  uploader.ResumeUploadFile(GURL(kTestUploadExistingFileURL),
                            local_path,
                            kTestMimeType,
                            test_util::CreateCopyResultCallback(
                                &error, &upload_location, &entry),
                            google_apis::ProgressCallback());
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(GDATA_NO_CONNECTION, error);
  EXPECT_TRUE(upload_location.is_empty());
}

TEST_F(DriveUploaderTest, NonExistingSourceFile) {
  GDataErrorCode error = GDATA_OTHER_ERROR;
  GURL upload_location;
  scoped_ptr<FileResource> entry;

  DriveUploader uploader(NULL,  // NULL, the service won't be used.
                         base::MessageLoopProxy::current().get());
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId,
      temp_dir_.path().AppendASCII("_this_path_should_not_exist_"),
      kTestMimeType,
      DriveUploader::UploadExistingFileOptions(),
      test_util::CreateCopyResultCallback(
          &error, &upload_location, &entry),
      google_apis::ProgressCallback());
  base::RunLoop().RunUntilIdle();

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
  scoped_ptr<FileResource> entry;

  MockDriveServiceWithUploadExpectation mock_service(local_path, data.size());
  DriveUploader uploader(&mock_service,
                         base::MessageLoopProxy::current().get());
  // Emulate the situation that the only first part is successfully uploaded,
  // but not the latter half.
  mock_service.set_received_bytes(512 * 1024);

  std::vector<test_util::ProgressInfo> upload_progress_values;
  uploader.ResumeUploadFile(
      GURL(kTestUploadExistingFileURL),
      local_path,
      kTestMimeType,
      test_util::CreateCopyResultCallback(
          &error, &upload_location, &entry),
      base::Bind(&test_util::AppendProgressCallbackResult,
                 &upload_progress_values));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(1, mock_service.resume_upload_call_count());
  EXPECT_EQ(1024 * 1024, mock_service.received_bytes());
  EXPECT_EQ(HTTP_SUCCESS, error);
  EXPECT_TRUE(upload_location.is_empty());
  ASSERT_TRUE(entry);
  EXPECT_EQ(kTestDummyMd5, entry->md5_checksum());
  ASSERT_EQ(1U, upload_progress_values.size());
  EXPECT_EQ(test_util::ProgressInfo(1024 * 1024, 1024 * 1024),
            upload_progress_values[0]);
}

}  // namespace drive
