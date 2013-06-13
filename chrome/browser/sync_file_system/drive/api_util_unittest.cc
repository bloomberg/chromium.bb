// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive/api_util.h"

#include "base/json/json_reader.h"
#include "base/location.h"
#include "base/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/values.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/drive_uploader.h"
#include "chrome/browser/google_apis/fake_drive_service.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/browser/sync_file_system/drive_file_sync_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/escape.h"
#include "testing/gtest/include/gtest/gtest.h"

#define FPL(x) FILE_PATH_LITERAL(x)

using google_apis::FakeDriveService;
using google_apis::ResourceEntry;
using google_apis::ResourceList;
using google_apis::DriveServiceInterface;
using google_apis::DriveUploaderInterface;
using google_apis::GDataErrorCode;
using google_apis::Link;
using google_apis::test_util::LoadJSONFile;

namespace sync_file_system {
namespace drive {

namespace {

const char kOrigin[] = "chrome-extension://example";
const char kOriginDirectoryName[] = "example";
const char kOriginDirectoryResourceId[] = "folder:origin_directory_resource_id";
const char kSyncRootResourceId[] = "folder:sync_root_resource_id";

void EmptyResourceEntryCallback(GDataErrorCode error,
                                scoped_ptr<ResourceEntry> entry) {}

void VerifyTitleUniqueness(const tracked_objects::Location& from_here,
                           const std::string& resource_id,
                           google_apis::DriveEntryKind kind,
                           GDataErrorCode error,
                           scoped_ptr<ResourceList> resource_list) {
  std::string location(" failed at " + from_here.ToString());
  ASSERT_FALSE(resource_id.empty()) << location;
  ASSERT_EQ(google_apis::HTTP_SUCCESS, error) << location;
  ASSERT_TRUE(resource_list) << location;

  const ScopedVector<ResourceEntry>& entries = resource_list->entries();
  ASSERT_EQ(1u, entries.size());
  EXPECT_EQ(resource_id, entries[0]->resource_id());
  switch (kind) {
    case google_apis::ENTRY_KIND_FOLDER:
      EXPECT_TRUE(entries[0]->is_folder()) << location;
      return;
    case google_apis::ENTRY_KIND_FILE:
      EXPECT_TRUE(entries[0]->is_file()) << location;
      return;
    default:
      NOTREACHED() << "Unexpected DriveEntryKind: " << kind << location;
      return;
  }
}

void VerifyFileDeletion(const tracked_objects::Location& from_here,
                        GDataErrorCode error,
                        scoped_ptr<ResourceList> resource_list) {
  std::string location(" failed at " + from_here.ToString());
  ASSERT_EQ(google_apis::HTTP_SUCCESS, error) << location;
  ASSERT_TRUE(resource_list) << location;
  EXPECT_TRUE(resource_list->entries().empty()) << location;
}

class FakeDriveServiceWrapper : public FakeDriveService {
 public:
  FakeDriveServiceWrapper() : make_directory_conflict_(false) {};
  virtual ~FakeDriveServiceWrapper() {};

  // DriveServiceInterface overrides.
  virtual void AddNewDirectory(
      const std::string& parent_resource_id,
      const std::string& directory_name,
      const google_apis::GetResourceEntryCallback& callback) OVERRIDE {
    if (make_directory_conflict_) {
      FakeDriveService::AddNewDirectory(
          parent_resource_id,
          directory_name,
          base::Bind(&EmptyResourceEntryCallback));
    }
    FakeDriveService::AddNewDirectory(
        parent_resource_id, directory_name, callback);
  }

  void set_make_directory_conflict(bool enable) {
    make_directory_conflict_ = enable;
  }

 private:
  bool make_directory_conflict_;

  DISALLOW_COPY_AND_ASSIGN(FakeDriveServiceWrapper);
};

// A fake implementation of DriveUploaderInterface, which provides fake
// behaviors for file uploading.
class FakeDriveUploader : public DriveUploaderInterface {
 public:
  explicit FakeDriveUploader(FakeDriveServiceWrapper* fake_drive_service)
      : fake_drive_service_(fake_drive_service),
        make_file_conflict_(false) {}
  virtual ~FakeDriveUploader() {}

  // DriveUploaderInterface overrides.

  // Pretends that a new file was uploaded successfully, and returns the
  // contents of "chromeos/gdata/file_entry.json" to the caller.
  virtual void UploadNewFile(
      const std::string& parent_resource_id,
      const base::FilePath& drive_file_path,
      const base::FilePath& local_file_path,
      const std::string& title,
      const std::string& content_type,
      const google_apis::UploadCompletionCallback& callback,
      const google_apis::ProgressCallback& progress_callback) OVERRIDE {
    DCHECK(!callback.is_null());

    scoped_ptr<base::Value> file_entry_data(
        LoadJSONFile("chromeos/gdata/file_entry.json"));
    scoped_ptr<ResourceEntry> file_entry(
        ResourceEntry::ExtractAndParse(*file_entry_data));

    if (make_file_conflict_) {
      fake_drive_service_->LoadResourceListForWapi(
          "chromeos/sync_file_system/conflict_with_file.json");
    } else {
      fake_drive_service_->LoadResourceListForWapi(
          "chromeos/sync_file_system/upload_new_file.json");
    }

    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   google_apis::HTTP_SUCCESS,
                   GURL(),
                   base::Passed(&file_entry)));
  }

  // Pretends that an existing file ("file:resource_id") was uploaded
  // successfully, and returns the contents of "chromeos/gdata/file_entry.json"
  // to the caller.
  virtual void UploadExistingFile(
      const std::string& resource_id,
      const base::FilePath& drive_file_path,
      const base::FilePath& local_file_path,
      const std::string& content_type,
      const std::string& etag,
      const google_apis::UploadCompletionCallback& callback,
      const google_apis::ProgressCallback& progress_callback) OVERRIDE {
    DCHECK(!callback.is_null());

    scoped_ptr<base::Value> file_entry_data(
        LoadJSONFile("chromeos/gdata/file_entry.json"));
    scoped_ptr<ResourceEntry> file_entry(
        ResourceEntry::ExtractAndParse(*file_entry_data));

    base::MessageLoopProxy::current()->PostTask(
        FROM_HERE,
        base::Bind(callback,
                   google_apis::HTTP_SUCCESS,
                   GURL(),
                   base::Passed(&file_entry)));
  }

  // At the moment, sync file system doesn't support resuming of the uploading.
  // So this method shouldn't be reached.
  virtual void ResumeUploadFile(
      const GURL& upload_location,
      const base::FilePath& drive_file_path,
      const base::FilePath& local_file_path,
      const std::string& content_type,
      const google_apis::UploadCompletionCallback& callback,
      const google_apis::ProgressCallback& progress_callback) OVERRIDE {
    NOTREACHED();
  }

  void set_make_file_conflict(bool enable) {
    make_file_conflict_ = enable;
  }

 private:
  FakeDriveServiceWrapper* fake_drive_service_;
  bool make_file_conflict_;

  DISALLOW_COPY_AND_ASSIGN(FakeDriveUploader);
};

}  // namespace

class APIUtilTest : public testing::Test {
 public:
  APIUtilTest() : ui_thread_(content::BrowserThread::UI, &message_loop_),
                  fake_drive_service_(NULL),
                  fake_drive_uploader_(NULL) {}

  virtual void SetUp() OVERRIDE {
    SetDisableDriveAPI(true);

    fake_drive_service_ = new FakeDriveServiceWrapper;
    fake_drive_uploader_ = new FakeDriveUploader(fake_drive_service_);

    api_util_ = APIUtil::CreateForTesting(
        &profile_,
        scoped_ptr<DriveServiceInterface>(fake_drive_service_),
        scoped_ptr<DriveUploaderInterface>(fake_drive_uploader_));
  }

  virtual void TearDown() OVERRIDE {
    api_util_.reset();
    SetDisableDriveAPI(false);
  }

 protected:
  std::string GetRootResourceId() { return api_util_->GetRootResourceId(); }

  APIUtil* api_util() { return api_util_.get(); }

  FakeDriveServiceWrapper* fake_drive_service() {
    return fake_drive_service_;
  }

  FakeDriveUploader* fake_drive_uploader() {
    return fake_drive_uploader_;
  }

  base::MessageLoop* message_loop() { return &message_loop_; }

 private:
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;

  TestingProfile profile_;
  scoped_ptr<APIUtil> api_util_;
  FakeDriveServiceWrapper* fake_drive_service_;
  FakeDriveUploader* fake_drive_uploader_;

  DISALLOW_COPY_AND_ASSIGN(APIUtilTest);
};

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

void DidGetResourceList(bool* done_out,
                        GDataErrorCode* error_out,
                        scoped_ptr<ResourceList>* document_feed_out,
                        GDataErrorCode error,
                        scoped_ptr<ResourceList> document_feed) {
  EXPECT_FALSE(*done_out);
  *done_out = true;
  *error_out = error;
  *document_feed_out = document_feed.Pass();
}

void DidDownloadFile(bool* done_out,
                     std::string* expected_file_md5_out,
                     GDataErrorCode* error_out,
                     GDataErrorCode error,
                     const std::string& file_md5,
                     int64 file_size,
                     const base::Time& updated_time) {
  EXPECT_FALSE(*done_out);
  *done_out = true;
  *error_out = error;
  *expected_file_md5_out = file_md5;
}

void DidUploadFile(bool* done_out,
                   GDataErrorCode* error_out,
                   std::string* resource_id_out,
                   GDataErrorCode error,
                   const std::string& resource_id,
                   const std::string& file_md5) {
  EXPECT_FALSE(*done_out);
  *done_out = true;
  *error_out = error;
  *resource_id_out = resource_id;
}

void DidDeleteFile(bool* done_out,
                   GDataErrorCode* error_out,
                   GDataErrorCode error) {
  EXPECT_FALSE(*done_out);
  *done_out = true;
  *error_out = error;
}

TEST_F(APIUtilTest, GetSyncRoot) {
  fake_drive_service()->LoadResourceListForWapi(
      "chromeos/sync_file_system/sync_root_found.json");

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  api_util()->GetDriveDirectoryForSyncRoot(
      base::Bind(&DidGetResourceID, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_EQ(kSyncRootResourceId, resource_id);
}

TEST_F(APIUtilTest, CreateSyncRoot) {
  fake_drive_service()->LoadResourceListForWapi(
      "chromeos/sync_file_system/sync_root_not_found.json");

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  api_util()->GetDriveDirectoryForSyncRoot(
      base::Bind(&DidGetResourceID, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_CREATED, error);
  EXPECT_FALSE(resource_id.empty());

  fake_drive_service()->SearchByTitle(
      APIUtil::GetSyncRootDirectoryName(),
      std::string(),  // directory_resource_id
      base::Bind(&VerifyTitleUniqueness,
                 FROM_HERE,
                 resource_id,
                 google_apis::ENTRY_KIND_FOLDER));
  message_loop()->RunUntilIdle();
}

TEST_F(APIUtilTest, CreateSyncRoot_Conflict) {
  fake_drive_service()->LoadResourceListForWapi(
      "chromeos/sync_file_system/sync_root_not_found.json");
  fake_drive_service()->set_make_directory_conflict(true);

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  api_util()->GetDriveDirectoryForSyncRoot(
      base::Bind(&DidGetResourceID, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_FALSE(resource_id.empty());

  // Verify that there is no duplicated directory on the remote side.
  fake_drive_service()->SearchByTitle(
      APIUtil::GetSyncRootDirectoryName(),
      std::string(),  // directory_resource_id
      base::Bind(&VerifyTitleUniqueness,
                 FROM_HERE,
                 resource_id,
                 google_apis::ENTRY_KIND_FOLDER));
  message_loop()->RunUntilIdle();
}

TEST_F(APIUtilTest, GetOriginDirectory) {
  fake_drive_service()->LoadResourceListForWapi(
      "chromeos/sync_file_system/origin_directory_found.json");

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  api_util()->GetDriveDirectoryForOrigin(
      kSyncRootResourceId,
      GURL(kOrigin),
      base::Bind(&DidGetResourceID, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_EQ(kOriginDirectoryResourceId, resource_id);
}

TEST_F(APIUtilTest, CreateOriginDirectory) {
  fake_drive_service()->LoadResourceListForWapi(
      "chromeos/sync_file_system/sync_root_found.json");

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  api_util()->GetDriveDirectoryForOrigin(
      kSyncRootResourceId,
      GURL(kOrigin),
      base::Bind(&DidGetResourceID, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_CREATED, error);
  EXPECT_FALSE(resource_id.empty());

  fake_drive_service()->SearchByTitle(
      kOriginDirectoryName,
      kSyncRootResourceId,
      base::Bind(&VerifyTitleUniqueness,
                 FROM_HERE,
                 resource_id,
                 google_apis::ENTRY_KIND_FOLDER));
  message_loop()->RunUntilIdle();
}

TEST_F(APIUtilTest, CreateOriginDirectory_Conflict) {
  fake_drive_service()->LoadResourceListForWapi(
      "chromeos/sync_file_system/sync_root_found.json");
  fake_drive_service()->set_make_directory_conflict(true);

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  api_util()->GetDriveDirectoryForOrigin(
      kSyncRootResourceId,
      GURL(kOrigin),
      base::Bind(&DidGetResourceID, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_FALSE(resource_id.empty());

  // Verify that there is no duplicated directory on the remote side.
  fake_drive_service()->SearchByTitle(
      kOriginDirectoryName,
      kSyncRootResourceId,
      base::Bind(&VerifyTitleUniqueness,
                 FROM_HERE,
                 resource_id,
                 google_apis::ENTRY_KIND_FOLDER));
  message_loop()->RunUntilIdle();
}

TEST_F(APIUtilTest, GetLargestChangeStamp) {
  fake_drive_service()->LoadAccountMetadataForWapi(
      "chromeos/sync_file_system/account_metadata.json");

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  int64 largest_changestamp = -1;
  api_util()->GetLargestChangeStamp(base::Bind(
      &DidGetLargestChangeStamp, &done, &error, &largest_changestamp));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_EQ(654321, largest_changestamp);
}

TEST_F(APIUtilTest, ListFiles) {
  fake_drive_service()->LoadResourceListForWapi(
      "chromeos/sync_file_system/listing_files_in_directory.json");

  fake_drive_service()->set_default_max_results(1);

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> document_feed;
  api_util()->ListFiles(
      kOriginDirectoryResourceId,
      base::Bind(&DidGetResourceList, &done, &error, &document_feed));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_FALSE(document_feed->entries().empty());

  GURL feed_url;
  ASSERT_TRUE(document_feed->GetNextFeedURL(&feed_url));

  done = false;
  error = google_apis::GDATA_OTHER_ERROR;
  document_feed.reset();

  api_util()->ContinueListing(
      feed_url, base::Bind(&DidGetResourceList, &done, &error, &document_feed));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_FALSE(document_feed->entries().empty());
}

TEST_F(APIUtilTest, ListChanges) {
  const int64 kStartChangestamp = 123456;

  fake_drive_service()->LoadResourceListForWapi(
      "chromeos/sync_file_system/listing_files_in_directory.json");

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> document_feed;
  api_util()->ListFiles(
      kOriginDirectoryResourceId,
      base::Bind(&DidGetResourceList, &done, &error, &document_feed));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_FALSE(document_feed->entries().empty());

  done = false;
  error = google_apis::GDATA_OTHER_ERROR;
  document_feed.reset();
  api_util()->ListChanges(
      kStartChangestamp,
      base::Bind(&DidGetResourceList, &done, &error, &document_feed));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_FALSE(document_feed->entries().empty());
}

TEST_F(APIUtilTest, DownloadFile) {
  const std::string kResourceId = "file:file_resource_id";
  const std::string kLocalFileMD5 = "123456";
  const std::string kExpectedFileMD5 = "3b4382ebefec6e743578c76bbd0575ce";

  fake_drive_service()->LoadResourceListForWapi(
      "chromeos/sync_file_system/listing_files_in_directory.json");

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath kOutputFilePath = temp_dir.path().AppendASCII("file");

  bool done = false;
  std::string file_md5;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  api_util()->DownloadFile(
      kResourceId,
      kLocalFileMD5,
      kOutputFilePath,
      base::Bind(&DidDownloadFile, &done, &file_md5, &error));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(kExpectedFileMD5, file_md5);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
}

TEST_F(APIUtilTest, DownloadFileInNotModified) {
  const std::string kResourceId = "file:file_resource_id";

  // Since local file's hash value is equal to remote file's one, it is expected
  // to cancel download the file and to return NOT_MODIFIED status code.
  const std::string kLocalFileMD5 = "3b4382ebefec6e743578c76bbd0575ce";

  fake_drive_service()->LoadResourceListForWapi(
      "chromeos/sync_file_system/listing_files_in_directory.json");

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath kOutputFilePath = temp_dir.path().AppendASCII("file");

  bool done = false;
  std::string file_md5;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  api_util()->DownloadFile(
      kResourceId,
      kLocalFileMD5,
      kOutputFilePath,
      base::Bind(&DidDownloadFile, &done, &file_md5, &error));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(kLocalFileMD5, file_md5);
  EXPECT_EQ(google_apis::HTTP_NOT_MODIFIED, error);
}

TEST_F(APIUtilTest, UploadNewFile) {
  const base::FilePath kLocalFilePath(FPL("/tmp/dir/file"));

  fake_drive_service()->LoadResourceListForWapi(
      "chromeos/sync_file_system/listing_files_in_directory.json");

  scoped_ptr<base::Value> file_entry_data(
      LoadJSONFile("chromeos/gdata/file_entry.json"));
  scoped_ptr<ResourceEntry> expected_file_entry(
      ResourceEntry::ExtractAndParse(*file_entry_data));

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  api_util()->UploadNewFile(
      kOriginDirectoryResourceId,
      kLocalFilePath,
      expected_file_entry->title(),
      base::Bind(&DidUploadFile, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_CREATED, error);
  EXPECT_EQ(expected_file_entry->resource_id(), resource_id);

  fake_drive_service()->SearchByTitle(
      expected_file_entry->title(),
      kOriginDirectoryResourceId,
      base::Bind(&VerifyTitleUniqueness,
                 FROM_HERE,
                 expected_file_entry->resource_id(),
                 google_apis::ENTRY_KIND_FILE));
  message_loop()->RunUntilIdle();
}

TEST_F(APIUtilTest, UploadNewFile_ConflictWithFile) {
  const base::FilePath kLocalFilePath(FPL("/tmp/dir/file"));

  fake_drive_service()->LoadResourceListForWapi(
      "chromeos/sync_file_system/upload_new_file.json");

  scoped_ptr<base::Value> file_entry_data(
      LoadJSONFile("chromeos/gdata/file_entry.json"));
  scoped_ptr<ResourceEntry> expected_file_entry(
      ResourceEntry::ExtractAndParse(*file_entry_data));

  fake_drive_uploader()->set_make_file_conflict(true);

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  api_util()->UploadNewFile(
      kOriginDirectoryResourceId,
      kLocalFilePath,
      expected_file_entry->title(),
      base::Bind(&DidUploadFile, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  // HTTP_CONFLICT error must be returned with empty resource_id.
  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_CONFLICT, error);
  EXPECT_EQ("file:file_duplicated_resource_id", resource_id);

  // Verify that there is no duplicated file on the remote side.
  fake_drive_service()->SearchByTitle(
      expected_file_entry->title(),
      kOriginDirectoryResourceId,
      base::Bind(&VerifyTitleUniqueness,
                 FROM_HERE,
                 resource_id,
                 google_apis::ENTRY_KIND_FILE));
  message_loop()->RunUntilIdle();
}

TEST_F(APIUtilTest, UploadExistingFile) {
  const base::FilePath kLocalFilePath(FPL("/tmp/dir/file"));

  fake_drive_service()->LoadResourceListForWapi(
      "chromeos/sync_file_system/upload_new_file.json");

  scoped_ptr<base::Value> file_entry_data(
      LoadJSONFile("chromeos/gdata/file_entry.json"));
  scoped_ptr<ResourceEntry> existing_file_entry(
      ResourceEntry::ExtractAndParse(*file_entry_data));

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  api_util()->UploadExistingFile(
      existing_file_entry->resource_id(),
      existing_file_entry->file_md5(),
      kLocalFilePath,
      base::Bind(&DidUploadFile, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_EQ(existing_file_entry->resource_id(), resource_id);

  fake_drive_service()->SearchByTitle(
      existing_file_entry->title(),
      kOriginDirectoryResourceId,
      base::Bind(&VerifyTitleUniqueness,
                 FROM_HERE,
                 existing_file_entry->resource_id(),
                 google_apis::ENTRY_KIND_FILE));
  message_loop()->RunUntilIdle();
}

TEST_F(APIUtilTest, UploadExistingFileInConflict) {
  const std::string kResourceId = "file:resource_id";
  const base::FilePath kLocalFilePath(FPL("/tmp/dir/file"));

  // Since remote file's hash value is different from the expected one, it is
  // expected to cancel upload the file and to return CONFLICT status code.
  const std::string kExpectedRemoteFileMD5 = "123456";

  fake_drive_service()->LoadResourceListForWapi(
      "chromeos/sync_file_system/upload_new_file.json");

  scoped_ptr<base::Value> file_entry_data(
      LoadJSONFile("chromeos/gdata/file_entry.json"));
  scoped_ptr<ResourceEntry> existing_file_entry(
      ResourceEntry::ExtractAndParse(*file_entry_data));

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  api_util()->UploadExistingFile(
      existing_file_entry->resource_id(),
      kExpectedRemoteFileMD5,
      kLocalFilePath,
      base::Bind(&DidUploadFile, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_CONFLICT, error);
  EXPECT_TRUE(resource_id.empty());

  // Verify that there is no duplicated file on the remote side.
  fake_drive_service()->SearchByTitle(
      existing_file_entry->title(),
      kOriginDirectoryResourceId,
      base::Bind(&VerifyTitleUniqueness,
                 FROM_HERE,
                 existing_file_entry->resource_id(),
                 google_apis::ENTRY_KIND_FILE));
  message_loop()->RunUntilIdle();
}

TEST_F(APIUtilTest, DeleteFile) {
  const std::string kFileTitle = "testfile";
  const std::string kResourceId = "file:file_resource_id";
  const std::string kExpectedRemoteFileMD5 = "3b4382ebefec6e743578c76bbd0575ce";

  fake_drive_service()->LoadResourceListForWapi(
      "chromeos/sync_file_system/listing_files_in_directory.json");

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  api_util()->DeleteFile(kResourceId,
                         kExpectedRemoteFileMD5,
                         base::Bind(&DidDeleteFile, &done, &error));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);

  fake_drive_service()->SearchByTitle(
      kFileTitle,
      kOriginDirectoryResourceId,
      base::Bind(&VerifyFileDeletion, FROM_HERE));
  message_loop()->RunUntilIdle();
}

TEST_F(APIUtilTest, DeleteFileInConflict) {
  const std::string kFileTitle = "testfile";
  const std::string kResourceId = "file:file_resource_id";

  fake_drive_service()->LoadResourceListForWapi(
      "chromeos/sync_file_system/listing_files_in_directory.json");

  // Since remote file's hash value is different from the expected one, it is
  // expected to cancel delete the file and to return CONFLICT status code.
  const std::string kExpectedRemoteFileMD5 = "123456";

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  api_util()->DeleteFile(kResourceId,
                         kExpectedRemoteFileMD5,
                         base::Bind(&DidDeleteFile, &done, &error));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_CONFLICT, error);

  // Verify that the conflict file was not deleted on the remote side.
  fake_drive_service()->SearchByTitle(
      kFileTitle,
      kOriginDirectoryResourceId,
      base::Bind(&VerifyTitleUniqueness,
                 FROM_HERE,
                 kResourceId,
                 google_apis::ENTRY_KIND_FILE));
  message_loop()->RunUntilIdle();
}

TEST_F(APIUtilTest, CreateDirectory) {
  const std::string kDirectoryTitle("directory");

  fake_drive_service()->LoadResourceListForWapi(
      "chromeos/sync_file_system/origin_directory_found.json");

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  api_util()->CreateDirectory(
      kOriginDirectoryResourceId,
      kDirectoryTitle,
      base::Bind(&DidGetResourceID, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_CREATED, error);
  EXPECT_FALSE(resource_id.empty());

  fake_drive_service()->SearchByTitle(
      kDirectoryTitle,
      kOriginDirectoryResourceId,
      base::Bind(&VerifyTitleUniqueness,
                 FROM_HERE,
                 resource_id,
                 google_apis::ENTRY_KIND_FOLDER));
  message_loop()->RunUntilIdle();
}

#endif  // !defined(OS_ANDROID)

}  // namespace drive
}  // namespace sync_file_system
