// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_file_sync_client.h"

#include "base/json/json_reader.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/google_apis/mock_drive_service.h"
#include "chrome/browser/google_apis/mock_drive_uploader.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/test/base/testing_profile.h"
#include "net/base/escape.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::StrictMock;
using ::testing::_;

using google_apis::DocumentFeed;
using google_apis::DriveServiceInterface;
using google_apis::DriveUploaderInterface;
using google_apis::GDataErrorCode;
using google_apis::MockDriveService;
using google_apis::MockDriveUploader;

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
    mock_drive_service_ = new StrictMock<MockDriveService>;
    mock_drive_uploader_ = new StrictMock<MockDriveUploader>;

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

  StrictMock<MockDriveService>* mock_drive_service() {
    return mock_drive_service_;
  }

  StrictMock<MockDriveUploader>* mock_drive_uploader() {
    return mock_drive_uploader_;
  }

  MessageLoop* message_loop() { return &message_loop_; }

 private:
  MessageLoop message_loop_;

  TestingProfile profile_;
  scoped_ptr<DriveFileSyncClient> sync_client_;
  StrictMock<MockDriveService>* mock_drive_service_;
  StrictMock<MockDriveUploader>* mock_drive_uploader_;

  DISALLOW_COPY_AND_ASSIGN(DriveFileSyncClientTest);
};

// Invokes |arg0| as a GetDataCallback.
ACTION_P2(InvokeGetDataCallback0, error, result) {
  scoped_ptr<base::Value> value(result.Pass());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(arg0, error, base::Passed(&value)));
}

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

// Invokes |arg5| as a GetDataCallback.
ACTION_P2(InvokeGetDataCallback5, error, result) {
  scoped_ptr<base::Value> value(result.Pass());
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(arg5, error, base::Passed(&value)));
}

void DidGetResourceID(bool* done_out,
                      GDataErrorCode* error_out,
                      std::string* resource_id_out,
                      GDataErrorCode error,
                      const std::string& resource_id) {
  EXPECT_FALSE(*done_out);
  *done_out = true;
  *error_out = error;
  *resource_id_out = resource_id;
}

#if !defined(OS_ANDROID)

void DidGetLargestChangeStamp(bool* done_out,
                              GDataErrorCode* error_out,
                              int64* largest_changestamp_out,
                              GDataErrorCode error,
                              int64 largest_changestamp) {
  EXPECT_FALSE(*done_out);
  *done_out = true;
  *error_out = error;
  *largest_changestamp_out = largest_changestamp;
}

void DidGetDocumentFeed(bool* done_out,
                        GDataErrorCode* error_out,
                        scoped_ptr<DocumentFeed>* document_feed_out,
                        GDataErrorCode error,
                        scoped_ptr<DocumentFeed> document_feed) {
  EXPECT_FALSE(*done_out);
  *done_out = true;
  *error_out = error;
  *document_feed_out = document_feed.Pass();
}

TEST_F(DriveFileSyncClientTest, GetSyncRoot) {
  scoped_ptr<base::Value> found_result(google_apis::test_util::LoadJSONFile(
      "sync_file_system/sync_root_found.json").Pass());

  // Expected to call GetDocuments from GetDriveDirectoryForSyncRoot.
  EXPECT_CALL(*mock_drive_service(),
              GetDocuments(GURL(),         // feed_url
                           0,              // start_changestamp,
                           FormatTitleQuery(kSyncRootDirectoryName),
                           false,          // shared_with_me
                           std::string(),  // directory_resource_id,
                           _))
      .WillOnce(InvokeGetDataCallback5(google_apis::HTTP_SUCCESS,
                                       base::Passed(&found_result)));

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
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
  scoped_ptr<base::Value> created_result(google_apis::test_util::LoadJSONFile(
      "sync_file_system/sync_root_created.json").Pass());

  // Expected to call GetDocuments from GetDriveDirectoryForSyncRoot.
  EXPECT_CALL(*mock_drive_service(),
              GetDocuments(GURL(),         // feed_urlc
                           0,              // start_changestamp
                           FormatTitleQuery(kSyncRootDirectoryName),
                           false,          // shared_with_me
                           std::string(),  // directory_resource_id
                           _))
      .WillOnce(InvokeGetDataCallback5(google_apis::HTTP_SUCCESS,
                                       base::Passed(&not_found_result)));

  // Expected to call AddNewDirectory from GetDriveDirectoryForSyncRoot.
  EXPECT_CALL(*mock_drive_service(),
              AddNewDirectory(
                  GURL(),  // content_url
                  FilePath().AppendASCII(kSyncRootDirectoryName).value(),
                  _))
      .WillOnce(InvokeGetDataCallback2(google_apis::HTTP_CREATED,
                                       base::Passed(&created_result)));

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
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
  const std::string kOriginDirectoryResourceId(
      "folder:origin_directory_resource_id");
  const GURL kOrigin("http://example.com");

  scoped_ptr<base::Value> found_result(google_apis::test_util::LoadJSONFile(
      "sync_file_system/origin_directory_found.json").Pass());

  // Expected to call GetDocuments from GetDriveDirectoryForOrigin.
  EXPECT_CALL(*mock_drive_service(),
              GetDocuments(GURL(),  // feed_url
                           0,       // start_changestamp
                           FormatTitleQuery(kOrigin.spec()),
                           false,   // shared_with_me
                           kParentResourceId,
                           _))
      .WillOnce(InvokeGetDataCallback5(google_apis::HTTP_SUCCESS,
                                       base::Passed(&found_result)));

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  sync_client()->GetDriveDirectoryForOrigin(
      kParentResourceId, kOrigin,
      base::Bind(&DidGetResourceID, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_EQ(kOriginDirectoryResourceId, resource_id);
}

TEST_F(DriveFileSyncClientTest, CreateOriginDirectory) {
  const std::string kParentResourceId("folder:sync_root_resource_id");
  const GURL kOrigin("http://example.com");

  scoped_ptr<base::Value> not_found_result(google_apis::test_util::LoadJSONFile(
      "sync_file_system/origin_directory_not_found.json").Pass());
  scoped_ptr<base::Value> got_parent_result(
      google_apis::test_util::LoadJSONFile(
          "sync_file_system/origin_directory_get_parent.json").Pass());
  scoped_ptr<base::Value> created_result(google_apis::test_util::LoadJSONFile(
      "sync_file_system/origin_directory_created.json").Pass());

  testing::InSequence sequence;

  // Expected to call GetDocuments from GetDriveDirectoryForOrigin.
  EXPECT_CALL(*mock_drive_service(),
              GetDocuments(GURL(),             // feed_url
                           0,                  // start_changestamp
                           FormatTitleQuery(kOrigin.spec()),
                           false,              // shared_with_me
                           kParentResourceId,  // directory_resource_id
                           _))
      .WillOnce(InvokeGetDataCallback5(google_apis::HTTP_SUCCESS,
                                       base::Passed(&not_found_result)));

  // Expected to call GetDocumentEntry from GetDriveDirectoryForOrigin.
  EXPECT_CALL(*mock_drive_service(),
              GetDocumentEntry(kParentResourceId, _))
      .WillOnce(InvokeGetDataCallback1(google_apis::HTTP_SUCCESS,
                                       base::Passed(&got_parent_result)));

  // Expected to call AddNewDirectory from GetDriveDirectoryForOrigin.
  EXPECT_CALL(*mock_drive_service(),
              AddNewDirectory(GURL("https://sync_root_content_url"),
                              FilePath().AppendASCII(kOrigin.spec()).value(),
                              _))
      .WillOnce(InvokeGetDataCallback2(google_apis::HTTP_CREATED,
                                       base::Passed(&created_result)));

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  sync_client()->GetDriveDirectoryForOrigin(
      kParentResourceId, kOrigin,
      base::Bind(&DidGetResourceID, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_CREATED, error);
  EXPECT_EQ("folder:origin_directory_resource_id", resource_id);
}

TEST_F(DriveFileSyncClientTest, GetLargestChangeStamp) {
  scoped_ptr<base::Value> result(google_apis::test_util::LoadJSONFile(
      "sync_file_system/account_metadata.json").Pass());

  // Expected to call GetAccountMetadata from GetLargestChangeStamp.
  EXPECT_CALL(*mock_drive_service(), GetAccountMetadata(_))
      .WillOnce(InvokeGetDataCallback0(google_apis::HTTP_SUCCESS,
                                       base::Passed(&result)))
      .RetiresOnSaturation();

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  int64 largest_changestamp = -1;
  sync_client()->GetLargestChangeStamp(
      base::Bind(&DidGetLargestChangeStamp,
                 &done, &error, &largest_changestamp));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_EQ(654321, largest_changestamp);
}

TEST_F(DriveFileSyncClientTest, ListFiles) {
  const std::string kDirectoryResourceId =
      "folder:origin_directory_resource_id";
  const GURL kFeedURL("listing_files_in_directory_first_page.json");

  scoped_ptr<base::Value> first_result(google_apis::test_util::LoadJSONFile(
      "sync_file_system/listing_files_in_directory.json").Pass());
  scoped_ptr<base::Value> following_result(google_apis::test_util::LoadJSONFile(
      "sync_file_system/listing_files_in_directory_second_page.json").Pass());

  testing::InSequence sequence;

  // Expected to call GetDocuments from ListFiles.
  EXPECT_CALL(*mock_drive_service(),
              GetDocuments(GURL(),         // feed_url
                           0,              // start_changestamp
                           std::string(),  // search_query
                           false,          // shared_with_me
                           kDirectoryResourceId,
                           _))
      .WillOnce(InvokeGetDataCallback5(google_apis::HTTP_SUCCESS,
                                       base::Passed(&first_result)))
      .RetiresOnSaturation();

  // Expected to call GetDocuments from ContinueListing.
  EXPECT_CALL(*mock_drive_service(),
              GetDocuments(kFeedURL,
                           0,              // start_changestamp
                           std::string(),  // search_query
                           false,          // shared_with_me
                           std::string(),  // directory_resource_id
                           _))
      .WillOnce(InvokeGetDataCallback5(google_apis::HTTP_SUCCESS,
                                       base::Passed(&following_result)))
      .RetiresOnSaturation();

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<DocumentFeed> document_feed;
  sync_client()->ListFiles(kDirectoryResourceId,
                           base::Bind(&DidGetDocumentFeed,
                                      &done, &error, &document_feed));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_FALSE(document_feed->entries().empty());

  done = false;
  error = google_apis::GDATA_OTHER_ERROR;
  document_feed.reset();
  sync_client()->ContinueListing(kFeedURL,
                                 base::Bind(&DidGetDocumentFeed,
                                            &done, &error, &document_feed));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_FALSE(document_feed->entries().empty());
}

TEST_F(DriveFileSyncClientTest, ListChanges) {
  const std::string kDirectoryResourceId =
      "folder:origin_directory_resource_id";
  const int64 kStartChangestamp = 123456;

  scoped_ptr<base::Value> first_result(google_apis::test_util::LoadJSONFile(
      "sync_file_system/listing_files_in_directory.json").Pass());
  scoped_ptr<base::Value> following_result(google_apis::test_util::LoadJSONFile(
      "sync_file_system/listing_changed_files_in_directory.json").Pass());

  testing::InSequence sequence;

  // Expected to call GetDocuments from ListFiles.
  EXPECT_CALL(*mock_drive_service(),
              GetDocuments(GURL(),
                           0,                     // start_changestamp
                           std::string(),         // search_query
                           false,                 // shared_with_me
                           kDirectoryResourceId,  // directory_resource_id
                           _))
      .WillOnce(InvokeGetDataCallback5(google_apis::HTTP_SUCCESS,
                                       base::Passed(&first_result)))
      .RetiresOnSaturation();

  // Expected to call GetDocuments from ListChanges.
  EXPECT_CALL(*mock_drive_service(),
              GetDocuments(GURL(),
                           kStartChangestamp,
                           std::string(),  // search_query
                           false,          // shared_with_me
                           std::string(),  // directory_resource_id
                           _))
      .WillOnce(InvokeGetDataCallback5(google_apis::HTTP_SUCCESS,
                                       base::Passed(&following_result)))
      .RetiresOnSaturation();

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<DocumentFeed> document_feed;
  sync_client()->ListFiles(kDirectoryResourceId,
                           base::Bind(&DidGetDocumentFeed,
                                      &done, &error, &document_feed));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_FALSE(document_feed->entries().empty());

  done = false;
  error = google_apis::GDATA_OTHER_ERROR;
  document_feed.reset();
  sync_client()->ListChanges(kStartChangestamp,
                             base::Bind(&DidGetDocumentFeed,
                                        &done, &error, &document_feed));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_FALSE(document_feed->entries().empty());
}

#endif  // !defined(OS_ANDROID)

}  // namespace sync_file_system
