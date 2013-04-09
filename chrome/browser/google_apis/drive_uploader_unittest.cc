// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/google_apis/drive_uploader.h"

#include <algorithm>
#include <cstdlib>
#include <string>

#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/values.h"
#include "chrome/browser/google_apis/dummy_drive_service.h"
#include "chrome/browser/google_apis/test_util.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/io_buffer.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace google_apis {

namespace {

const char kTestDummyId[] = "file:dummy_id";
const char kTestDocumentTitle[] = "Hello world";
const char kTestDrivePath[] = "drive/dummy.txt";
const char kTestInitiateUploadParentResourceId[] = "parent_resource_id";
const char kTestInitiateUploadResourceId[] = "resource_id";
const char kTestMimeType[] = "text/plain";
const char kTestUploadURL[] = "http://test/upload_location";
const int64 kUploadChunkSize = 512 * 1024;
const char kTestETag[] = "test_etag";

// Creates a |size| byte file and returns its |path|. The file is filled with
// random bytes so that the test assertions can identify correct
// portion of the file is being sent.
bool CreateFileOfSpecifiedSize(const base::FilePath& temp_dir,
                               size_t size,
                               base::FilePath* path,
                               std::string* data) {
  data->resize(size);
  for (size_t i = 0; i < size; ++i)
    (*data)[i] = static_cast<char>(rand() % 256);  // NOLINT
  if (!file_util::CreateTemporaryFileInDir(temp_dir, path))
    return false;
  return file_util::WriteFile(*path, data->c_str(), static_cast<int>(size)) ==
      static_cast<int>(size);
}

// Mock DriveService that verifies if the uploaded content matches the preset
// expectation.
class MockDriveServiceWithUploadExpectation : public DummyDriveService {
 public:
  // Sets up an expected upload content. InitiateUpload and ResumeUpload will
  // verify that the specified data is correctly uploaded.
  explicit MockDriveServiceWithUploadExpectation(
      const std::string& expected_upload_content)
     : expected_upload_content_(expected_upload_content),
       received_bytes_(0),
       resume_upload_call_count_(0) {}

  int64 received_bytes() const { return received_bytes_; }

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
    const int64 expected_size = expected_upload_content_.size();
    EXPECT_EQ(expected_size, content_length);
    EXPECT_EQ(kTestInitiateUploadParentResourceId, parent_resource_id);

    // Calls back the upload URL for subsequent ResumeUpload operations.
    // InitiateUpload is an asynchronous function, so don't callback directly.
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback, HTTP_SUCCESS, GURL(kTestUploadURL)));
  }

  virtual void InitiateUploadExistingFile(
      const base::FilePath& drive_file_path,
      const std::string& content_type,
      int64 content_length,
      const std::string& resource_id,
      const std::string& etag,
      const InitiateUploadCallback& callback) OVERRIDE {
    EXPECT_EQ(kTestMimeType, content_type);
    const int64 expected_size = expected_upload_content_.size();
    EXPECT_EQ(expected_size, content_length);
    EXPECT_EQ(kTestInitiateUploadResourceId, resource_id);

    if (!etag.empty() && etag != kTestETag) {
      MessageLoop::current()->PostTask(FROM_HERE,
          base::Bind(callback, HTTP_PRECONDITION, GURL()));
      return;
    }

    // Calls back the upload URL for subsequent ResumeUpload operations.
    // InitiateUpload is an asynchronous function, so don't callback directly.
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback, HTTP_SUCCESS, GURL(kTestUploadURL)));
  }

  // Handles a request for uploading a chunk of bytes.
  virtual void ResumeUpload(
      UploadMode upload_mode,
      const base::FilePath& drive_file_path,
      const GURL& upload_url,
      int64 start_position,
      int64 end_position,
      int64 content_length,
      const std::string& content_type,
      const scoped_refptr<net::IOBuffer>& buf,
      const UploadRangeCallback& callback) OVERRIDE {
    const int64 expected_size = expected_upload_content_.size();

    // The upload range should start from the current first unreceived byte.
    EXPECT_EQ(received_bytes_, start_position);

    // The upload data must be split into 512KB chunks.
    const int64 expected_chunk_end =
        std::min(received_bytes_ + kUploadChunkSize, expected_size);
    EXPECT_EQ(expected_chunk_end, end_position);

    const int64 expected_chunk_size = expected_chunk_end - received_bytes_;
    const std::string expected_chunk_data(
        expected_upload_content_.substr(received_bytes_,
                                        expected_chunk_size));
    std::string uploading_data(buf->data(), buf->data() + expected_chunk_size);
    EXPECT_EQ(expected_chunk_data, uploading_data);

    // The upload URL returned by InitiateUpload() must be used.
    EXPECT_EQ(GURL(kTestUploadURL), upload_url);

    // Other parameters should be the exact values passed to DriveUploader.
    EXPECT_EQ(expected_size, content_length);
    EXPECT_EQ(kTestMimeType, content_type);

    // Update the internal status of the current upload session.
    resume_upload_call_count_++;
    received_bytes_ = end_position;

    // Callback with response.
    UploadRangeResponse response;
    scoped_ptr<ResourceEntry> entry;
    if (received_bytes_ == content_length) {
      response = UploadRangeResponse(
          upload_mode == UPLOAD_NEW_FILE ? HTTP_CREATED : HTTP_SUCCESS,
          -1, -1);

      base::DictionaryValue dict;
      dict.Set("id.$t", new base::StringValue(kTestDummyId));
      entry = ResourceEntry::CreateFrom(dict);
    } else {
      response = UploadRangeResponse(HTTP_RESUME_INCOMPLETE, 0, end_position);
    }
    // ResumeUpload is an asynchronous function, so don't callback directly.
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback, response, base::Passed(&entry)));
  }

  std::string expected_upload_content_;
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
      UploadMode upload_mode,
      const base::FilePath& drive_file_path,
      const GURL& upload_url,
      int64 start_position,
      int64 end_position,
      int64 content_length,
      const std::string& content_type,
      const scoped_refptr<net::IOBuffer>& buf,
      const UploadRangeCallback& callback) OVERRIDE {
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
        base::Bind(callback, HTTP_SUCCESS, GURL(kTestUploadURL)));
  }

  virtual void InitiateUploadExistingFile(
      const base::FilePath& drive_file_path,
      const std::string& content_type,
      int64 content_length,
      const std::string& resource_id,
      const std::string& etag,
      const InitiateUploadCallback& callback) OVERRIDE {
    MessageLoop::current()->PostTask(FROM_HERE,
        base::Bind(callback, HTTP_SUCCESS, GURL(kTestUploadURL)));
  }

  // Returns error.
  virtual void ResumeUpload(
      UploadMode upload_mode,
      const base::FilePath& drive_file_path,
      const GURL& upload_url,
      int64 start_position,
      int64 end_position,
      int64 content_length,
      const std::string& content_type,
      const scoped_refptr<net::IOBuffer>& buf,
      const UploadRangeCallback& callback) OVERRIDE {
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
  ASSERT_TRUE(CreateFileOfSpecifiedSize(temp_dir_.path(), 0,
                                        &local_path, &data));

  DriveUploadError error = DRIVE_UPLOAD_ERROR_ABORT;
  base::FilePath drive_path;
  base::FilePath file_path;
  scoped_ptr<ResourceEntry> resource_entry;

  MockDriveServiceWithUploadExpectation mock_service(data);
  DriveUploader uploader(&mock_service);
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId,
      base::FilePath::FromUTF8Unsafe(kTestDrivePath),
      local_path,
      kTestMimeType,
      "",  // etag
      test_util::CreateCopyResultCallback(
          &error, &drive_path, &file_path, &resource_entry));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(1, mock_service.resume_upload_call_count());
  EXPECT_EQ(0, mock_service.received_bytes());
  EXPECT_EQ(DRIVE_UPLOAD_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe(kTestDrivePath), drive_path);
  EXPECT_EQ(local_path, file_path);
  ASSERT_TRUE(resource_entry);
  EXPECT_EQ(kTestDummyId, resource_entry->id());
}

TEST_F(DriveUploaderTest, UploadExisting512KB) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(CreateFileOfSpecifiedSize(temp_dir_.path(), 512 * 1024,
                                        &local_path, &data));

  DriveUploadError error = DRIVE_UPLOAD_ERROR_ABORT;
  base::FilePath drive_path;
  base::FilePath file_path;
  scoped_ptr<ResourceEntry> resource_entry;

  MockDriveServiceWithUploadExpectation mock_service(data);
  DriveUploader uploader(&mock_service);
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId,
      base::FilePath::FromUTF8Unsafe(kTestDrivePath),
      local_path,
      kTestMimeType,
      "",  // etag
      test_util::CreateCopyResultCallback(
          &error, &drive_path, &file_path, &resource_entry));
  test_util::RunBlockingPoolTask();

  // 512KB upload should not be split into multiple chunks.
  EXPECT_EQ(1, mock_service.resume_upload_call_count());
  EXPECT_EQ(512 * 1024, mock_service.received_bytes());
  EXPECT_EQ(DRIVE_UPLOAD_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe(kTestDrivePath), drive_path);
  EXPECT_EQ(local_path, file_path);
  ASSERT_TRUE(resource_entry);
  EXPECT_EQ(kTestDummyId, resource_entry->id());
}

TEST_F(DriveUploaderTest, UploadExisting1234KB) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(CreateFileOfSpecifiedSize(temp_dir_.path(), 1234 * 1024,
                                        &local_path, &data));

  DriveUploadError error = DRIVE_UPLOAD_ERROR_ABORT;
  base::FilePath drive_path;
  base::FilePath file_path;
  scoped_ptr<ResourceEntry> resource_entry;

  MockDriveServiceWithUploadExpectation mock_service(data);
  DriveUploader uploader(&mock_service);
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId,
      base::FilePath::FromUTF8Unsafe(kTestDrivePath),
      local_path,
      kTestMimeType,
      "",  // etag
      test_util::CreateCopyResultCallback(
          &error, &drive_path, &file_path, &resource_entry));
  test_util::RunBlockingPoolTask();

  // The file should be split into 3 chunks (1234 = 512 + 512 + 210).
  EXPECT_EQ(3, mock_service.resume_upload_call_count());
  EXPECT_EQ(1234 * 1024, mock_service.received_bytes());
  EXPECT_EQ(DRIVE_UPLOAD_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe(kTestDrivePath), drive_path);
  EXPECT_EQ(local_path, file_path);
  ASSERT_TRUE(resource_entry);
  EXPECT_EQ(kTestDummyId, resource_entry->id());
}

TEST_F(DriveUploaderTest, UploadNew1234KB) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(CreateFileOfSpecifiedSize(temp_dir_.path(), 1234 * 1024,
                                        &local_path, &data));

  DriveUploadError error = DRIVE_UPLOAD_ERROR_ABORT;
  base::FilePath drive_path;
  base::FilePath file_path;
  scoped_ptr<ResourceEntry> resource_entry;

  MockDriveServiceWithUploadExpectation mock_service(data);
  DriveUploader uploader(&mock_service);
  uploader.UploadNewFile(
      kTestInitiateUploadParentResourceId,
      base::FilePath::FromUTF8Unsafe(kTestDrivePath),
      local_path,
      kTestDocumentTitle,
      kTestMimeType,
      test_util::CreateCopyResultCallback(
          &error, &drive_path, &file_path, &resource_entry));
  test_util::RunBlockingPoolTask();

  // The file should be split into 3 chunks (1234 = 512 + 512 + 210).
  EXPECT_EQ(3, mock_service.resume_upload_call_count());
  EXPECT_EQ(1234 * 1024, mock_service.received_bytes());
  EXPECT_EQ(DRIVE_UPLOAD_OK, error);
  EXPECT_EQ(base::FilePath::FromUTF8Unsafe(kTestDrivePath), drive_path);
  EXPECT_EQ(local_path, file_path);
  ASSERT_TRUE(resource_entry);
  EXPECT_EQ(kTestDummyId, resource_entry->id());
}

TEST_F(DriveUploaderTest, InitiateUploadFail) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(CreateFileOfSpecifiedSize(temp_dir_.path(), 512 * 1024,
                                        &local_path, &data));

  DriveUploadError error = DRIVE_UPLOAD_OK;
  base::FilePath drive_path;
  base::FilePath file_path;
  scoped_ptr<ResourceEntry> resource_entry;

  MockDriveServiceNoConnectionAtInitiate mock_service;
  DriveUploader uploader(&mock_service);
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId,
      base::FilePath::FromUTF8Unsafe(kTestDrivePath),
      local_path,
      kTestMimeType,
      "",  // etag
      test_util::CreateCopyResultCallback(
          &error, &drive_path, &file_path, &resource_entry));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_UPLOAD_ERROR_ABORT, error);
}

TEST_F(DriveUploaderTest, InitiateUploadNoConflict) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(CreateFileOfSpecifiedSize(temp_dir_.path(), 512 * 1024,
                                        &local_path, &data));

  DriveUploadError error = DRIVE_UPLOAD_ERROR_ABORT;
  base::FilePath drive_path;
  base::FilePath file_path;
  scoped_ptr<ResourceEntry> resource_entry;

  MockDriveServiceWithUploadExpectation mock_service(data);
  DriveUploader uploader(&mock_service);
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId,
      base::FilePath::FromUTF8Unsafe(kTestDrivePath),
      local_path,
      kTestMimeType,
      kTestETag,
      test_util::CreateCopyResultCallback(
          &error, &drive_path, &file_path, &resource_entry));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_UPLOAD_OK, error);
}

TEST_F(DriveUploaderTest, InitiateUploadConflict) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(CreateFileOfSpecifiedSize(temp_dir_.path(), 512 * 1024,
                                        &local_path, &data));
  const std::string kDestinationETag("destination_etag");

  DriveUploadError error = DRIVE_UPLOAD_ERROR_ABORT;
  base::FilePath drive_path;
  base::FilePath file_path;
  scoped_ptr<ResourceEntry> resource_entry;

  MockDriveServiceWithUploadExpectation mock_service(data);
  DriveUploader uploader(&mock_service);
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId,
      base::FilePath::FromUTF8Unsafe(kTestDrivePath),
      local_path,
      kTestMimeType,
      kDestinationETag,
      test_util::CreateCopyResultCallback(
          &error, &drive_path, &file_path, &resource_entry));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_UPLOAD_ERROR_CONFLICT, error);
}

TEST_F(DriveUploaderTest, ResumeUploadFail) {
  base::FilePath local_path;
  std::string data;
  ASSERT_TRUE(CreateFileOfSpecifiedSize(temp_dir_.path(), 512 * 1024,
                                        &local_path, &data));

  DriveUploadError error = DRIVE_UPLOAD_OK;
  base::FilePath drive_path;
  base::FilePath file_path;
  scoped_ptr<ResourceEntry> resource_entry;

  MockDriveServiceNoConnectionAtResume mock_service;
  DriveUploader uploader(&mock_service);
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId,
      base::FilePath::FromUTF8Unsafe(kTestDrivePath),
      local_path,
      kTestMimeType,
      "",  // etag
      test_util::CreateCopyResultCallback(
          &error, &drive_path, &file_path, &resource_entry));
  test_util::RunBlockingPoolTask();

  EXPECT_EQ(DRIVE_UPLOAD_ERROR_ABORT, error);
}

TEST_F(DriveUploaderTest, NonExistingSourceFile) {
  DriveUploadError error = DRIVE_UPLOAD_ERROR_ABORT;
  base::FilePath drive_path;
  base::FilePath file_path;
  scoped_ptr<ResourceEntry> resource_entry;

  DriveUploader uploader(NULL);  // NULL, the service won't be used.
  uploader.UploadExistingFile(
      kTestInitiateUploadResourceId,
      base::FilePath::FromUTF8Unsafe(kTestDrivePath),
      temp_dir_.path().AppendASCII("_this_path_should_not_exist_"),
      kTestMimeType,
      "",  // etag
      test_util::CreateCopyResultCallback(
          &error, &drive_path, &file_path, &resource_entry));
  test_util::RunBlockingPoolTask();

  // Should return failure without doing any attempt to connect to the server.
  EXPECT_EQ(DRIVE_UPLOAD_ERROR_NOT_FOUND, error);
}

}  // namespace google_apis
