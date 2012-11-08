// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_file_sync_client.h"

#include "base/json/json_reader.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/google_apis/gdata_test_util.h"
#include "chrome/browser/google_apis/mock_drive_service.h"
#include "chrome/browser/google_apis/mock_drive_uploader.h"
#include "chrome/test/base/testing_profile.h"
#include "net/base/escape.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrictMock;
using ::testing::_;

using google_apis::DriveServiceInterface;
using google_apis::DriveUploaderInterface;

namespace sync_file_system {

namespace {
const char kSyncRootDirectoryName[] = "Chrome Syncable FileSystem";
}  // namespace

class DriveFileSyncClientTest : public testing::Test {
 public:
  DriveFileSyncClientTest()
      : mock_drive_service_(NULL),
        mock_drive_uploader_(NULL) {
  }

  virtual void SetUp() OVERRIDE {
    mock_drive_service_ = new StrictMock<google_apis::MockDriveService>;
    mock_drive_uploader_ = new StrictMock<google_apis::MockDriveUploader>;

    EXPECT_CALL(*mock_drive_service_, Initialize(&profile_)).Times(1);

    sync_client_ = DriveFileSyncClient::CreateForTesting(
        &profile_,
        scoped_ptr<DriveServiceInterface>(mock_drive_service_),
        scoped_ptr<DriveUploaderInterface>(mock_drive_uploader_)).Pass();
  }

  virtual void TearDown() OVERRIDE {
    sync_client_.reset();
  }

 protected:
  DriveFileSyncClient* sync_client() { return sync_client_.get(); }

  std::string FormatTitleQuery(const std::string& title) {
    return DriveFileSyncClient::FormatTitleQuery(title);
  }

  StrictMock<google_apis::MockDriveService>* mock_drive_service() {
    return mock_drive_service_;
  }

  StrictMock<google_apis::MockDriveUploader>* mock_drive_uploader() {
    return mock_drive_uploader_;
  }

  MessageLoop* message_loop() { return &message_loop_; }

 private:
  MessageLoop message_loop_;

  TestingProfile profile_;
  scoped_ptr<DriveFileSyncClient> sync_client_;
  StrictMock<google_apis::MockDriveService>* mock_drive_service_;
  StrictMock<google_apis::MockDriveUploader>* mock_drive_uploader_;

  DISALLOW_COPY_AND_ASSIGN(DriveFileSyncClientTest);
};

// Invokes |arg1| as a GetDataCallback.
ACTION_P2(InvokeGetDataCallback1, error, result) {
  scoped_ptr<base::Value> value(result.Pass());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(arg1, error, base::Passed(&value)));
}

// Invokes |arg2| as a GetDataCallback.
ACTION_P2(InvokeGetDataCallback2, error, result) {
  scoped_ptr<base::Value> value(result.Pass());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(arg2, error, base::Passed(&value)));
}

// Invokes |arg4| as a GetDataCallback.
ACTION_P2(InvokeGetDataCallback4, error, result) {
  scoped_ptr<base::Value> value(result.Pass());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(arg4, error, base::Passed(&value)));
}

void DidGetResourceID(bool* done_out,
                      google_apis::GDataErrorCode* error_out,
                      std::string* resource_id_out,
                      google_apis::GDataErrorCode error,
                      const std::string& resource_id) {
  EXPECT_FALSE(*done_out);
  *done_out = true;
  *error_out = error;
  *resource_id_out = resource_id;
}

#if !defined(OS_ANDROID)

TEST_F(DriveFileSyncClientTest, GetSyncRoot) {
  scoped_ptr<base::Value> found_result(google_apis::test_util::LoadJSONFile(
      "sync_file_system/sync_root_found.json").Pass());
  EXPECT_CALL(*mock_drive_service(),
              GetDocuments(GURL() /* feed_url */,
                           0 /* start_changestamp */,
                           FormatTitleQuery(kSyncRootDirectoryName),
                           std::string() /* directory_resource_id */,
                           _))
      .WillOnce(InvokeGetDataCallback4(google_apis::HTTP_SUCCESS,
                                       base::Passed(&found_result)));

  bool done = false;
  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  sync_client()->GetDriveDirectoryForSyncRoot(
      base::Bind(&DidGetResourceID, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_EQ("folder:sync_root_resource_id", resource_id);
}

TEST_F(DriveFileSyncClientTest, CreateSyncRoot) {
  scoped_ptr<base::Value> not_found_result(google_apis::test_util::LoadJSONFile(
      "sync_file_system/sync_root_not_found.json").Pass());
  EXPECT_CALL(*mock_drive_service(),
              GetDocuments(GURL() /* feed_url */,
                           0 /* start_changestamp */,
                           FormatTitleQuery(kSyncRootDirectoryName),
                           std::string() /* directory_resource_id */,
                           _))
      .WillOnce(InvokeGetDataCallback4(google_apis::HTTP_SUCCESS,
                                       base::Passed(&not_found_result)));


  scoped_ptr<base::Value> created_result(google_apis::test_util::LoadJSONFile(
      "sync_file_system/sync_root_created.json").Pass());
  EXPECT_CALL(*mock_drive_service(),
              AddNewDirectory(
                  GURL() /* content_url */,
                  FilePath().AppendASCII(kSyncRootDirectoryName).value(),
                  _))
      .WillOnce(InvokeGetDataCallback2(google_apis::HTTP_CREATED,
                                       base::Passed(&created_result)));

  bool done = false;
  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  sync_client()->GetDriveDirectoryForSyncRoot(
      base::Bind(&DidGetResourceID, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_CREATED, error);
  EXPECT_EQ("folder:sync_root_resource_id", resource_id);
}

TEST_F(DriveFileSyncClientTest, GetOriginDirectory) {
  const std::string kParentResourceId("folder:sync_root_resource_id");
  const GURL kOrigin("http://example.com");

  scoped_ptr<base::Value> found_result(google_apis::test_util::LoadJSONFile(
      "sync_file_system/origin_directory_found.json").Pass());
  EXPECT_CALL(*mock_drive_service(),
              GetDocuments(GURL() /* feed_url */,
                           0 /* start_changestamp */,
                           FormatTitleQuery(kOrigin.spec()),
                           kParentResourceId,
                           _))
      .WillOnce(InvokeGetDataCallback4(google_apis::HTTP_SUCCESS,
                                       base::Passed(&found_result)));

  bool done = false;
  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  sync_client()->GetDriveDirectoryForOrigin(
      kParentResourceId, kOrigin,
      base::Bind(&DidGetResourceID, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_EQ("folder:origin_directory_resource_id", resource_id);
}

TEST_F(DriveFileSyncClientTest, CreateOriginDirectory) {
  const std::string kParentResourceId("folder:sync_root_resource_id");
  const GURL kOrigin("http://example.com");

  scoped_ptr<base::Value> not_found_result(google_apis::test_util::LoadJSONFile(
      "sync_file_system/origin_directory_not_found.json").Pass());
  EXPECT_CALL(*mock_drive_service(),
              GetDocuments(GURL() /* feed_url */,
                           0 /* start_changestamp */,
                           FormatTitleQuery(kOrigin.spec()),
                           kParentResourceId /* directory_resource_id */,
                           _))
      .WillOnce(InvokeGetDataCallback4(google_apis::HTTP_SUCCESS,
                                       base::Passed(&not_found_result)));

  scoped_ptr<base::Value> got_parent_result(
      google_apis::test_util::LoadJSONFile(
          "sync_file_system/origin_directory_get_parent.json"));
  EXPECT_CALL(*mock_drive_service(),
              GetDocumentEntry(kParentResourceId, _))
      .WillOnce(InvokeGetDataCallback1(google_apis::HTTP_SUCCESS,
                                       base::Passed(&got_parent_result)));

  scoped_ptr<base::Value> created_result(google_apis::test_util::LoadJSONFile(
      "sync_file_system/origin_directory_created.json").Pass());
  EXPECT_CALL(*mock_drive_service(),
              AddNewDirectory(GURL("https://sync_root_content_url"),
                              FilePath().AppendASCII(kOrigin.spec()).value(),
                              _))
      .WillOnce(InvokeGetDataCallback2(google_apis::HTTP_CREATED,
                                       base::Passed(&created_result)));

  bool done = false;
  google_apis::GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  sync_client()->GetDriveDirectoryForOrigin(
      kParentResourceId, kOrigin,
      base::Bind(&DidGetResourceID, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_CREATED, error);
  EXPECT_EQ("folder:origin_directory_resource_id", resource_id);
}

#endif  // !defined(OS_ANDROID)

}  // namespace sync_file_system
