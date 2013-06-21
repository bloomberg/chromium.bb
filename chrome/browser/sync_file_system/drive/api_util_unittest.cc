// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive/api_util.h"

#include "base/location.h"
#include "base/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/drive/drive_uploader.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/google_apis/drive_api_parser.h"
#include "chrome/browser/google_apis/gdata_errorcode.h"
#include "chrome/browser/google_apis/test_util.h"
#include "chrome/browser/sync_file_system/drive_file_sync_util.h"
#include "chrome/test/base/testing_profile.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/test_browser_thread.h"
#include "net/base/escape.h"
#include "testing/gtest/include/gtest/gtest.h"

#define FPL(x) FILE_PATH_LITERAL(x)

using ::drive::DriveServiceInterface;
using ::drive::DriveUploaderInterface;
using ::drive::FakeDriveService;
using ::drive::UploadCompletionCallback;
using google_apis::GDataErrorCode;
using google_apis::ProgressCallback;
using google_apis::ResourceEntry;
using google_apis::ResourceList;

namespace sync_file_system {
namespace drive {

namespace {

const char kOrigin[] = "chrome-extension://example";
const char kOriginDirectoryName[] = "example";

void DidRemoveResourceFromDirectory(GDataErrorCode error) {
  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
}

void DidAddFileOrDirectoryForMakingConflict(GDataErrorCode error,
                                            scoped_ptr<ResourceEntry> entry) {
  ASSERT_EQ(google_apis::HTTP_CREATED, error);
  ASSERT_TRUE(entry);
}

void DidAddNewDirectory(std::string* resource_id_out,
                        GDataErrorCode error,
                        scoped_ptr<ResourceEntry> entry) {
  ASSERT_TRUE(resource_id_out);
  ASSERT_EQ(google_apis::HTTP_CREATED, error);
  ASSERT_TRUE(entry);
  *resource_id_out = entry->resource_id();
}

void DidAddNewFile(std::string* resource_id_out,
                   std::string* file_md5_out,
                   GDataErrorCode error,
                   scoped_ptr<ResourceEntry> entry) {
  ASSERT_TRUE(resource_id_out);
  ASSERT_TRUE(file_md5_out);
  ASSERT_EQ(google_apis::HTTP_CREATED, error);
  ASSERT_TRUE(entry);
  *resource_id_out = entry->resource_id();
  *file_md5_out = entry->file_md5();
}

void DidAddFileForUploadNew(
    const UploadCompletionCallback& callback,
    GDataErrorCode error,
    scoped_ptr<ResourceEntry> entry) {
  ASSERT_EQ(google_apis::HTTP_CREATED, error);
  ASSERT_TRUE(entry);
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 google_apis::HTTP_SUCCESS,
                 GURL(),
                 base::Passed(&entry)));
}

void DidGetResourceEntryForUploadExisting(
    const UploadCompletionCallback& callback,
    GDataErrorCode error,
    scoped_ptr<ResourceEntry> entry) {
  ASSERT_EQ(google_apis::HTTP_SUCCESS, error);
  ASSERT_TRUE(entry);
  base::MessageLoopProxy::current()->PostTask(
      FROM_HERE,
      base::Bind(callback,
                 google_apis::HTTP_SUCCESS,
                 GURL(),
                 base::Passed(&entry)));
}

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
  virtual google_apis::CancelCallback AddNewDirectory(
      const std::string& parent_resource_id,
      const std::string& directory_name,
      const google_apis::GetResourceEntryCallback& callback) OVERRIDE {
    if (make_directory_conflict_) {
      FakeDriveService::AddNewDirectory(
          parent_resource_id,
          directory_name,
          base::Bind(&DidAddFileOrDirectoryForMakingConflict));
    }
    return FakeDriveService::AddNewDirectory(
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

  // Proxies a request to upload a new file to FakeDriveService, and returns the
  // resource entry to the caller.
  virtual google_apis::CancelCallback UploadNewFile(
      const std::string& parent_resource_id,
      const base::FilePath& local_file_path,
      const std::string& title,
      const std::string& content_type,
      const UploadCompletionCallback& callback,
      const ProgressCallback& progress_callback) OVERRIDE {
    DCHECK(!callback.is_null());
    const std::string kFileContent = "test content";

    if (make_file_conflict_) {
      fake_drive_service_->AddNewFile(
          content_type,
          kFileContent,
          parent_resource_id,
          title,
          false,  // shared_with_me
          base::Bind(&DidAddFileOrDirectoryForMakingConflict));
    }

    fake_drive_service_->AddNewFile(
        content_type,
        kFileContent,
        parent_resource_id,
        title,
        false,  // shared_with_me
        base::Bind(&DidAddFileForUploadNew, callback));
    base::MessageLoop::current()->RunUntilIdle();

    return google_apis::CancelCallback();
  }

  // Pretends that an existing file |resource_id| was uploaded successfully, and
  // returns a resource entry to the caller.
  virtual google_apis::CancelCallback UploadExistingFile(
      const std::string& resource_id,
      const base::FilePath& local_file_path,
      const std::string& content_type,
      const std::string& etag,
      const UploadCompletionCallback& callback,
      const ProgressCallback& progress_callback) OVERRIDE {
    DCHECK(!callback.is_null());
    return fake_drive_service_->GetResourceEntry(
        resource_id,
        base::Bind(&DidGetResourceEntryForUploadExisting, callback));
  }

  // At the moment, sync file system doesn't support resuming of the uploading.
  // So this method shouldn't be reached.
  virtual google_apis::CancelCallback ResumeUploadFile(
      const GURL& upload_location,
      const base::FilePath& local_file_path,
      const std::string& content_type,
      const UploadCompletionCallback& callback,
      const ProgressCallback& progress_callback) OVERRIDE {
    NOTREACHED();
    return google_apis::CancelCallback();
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
    fake_drive_service_ = new FakeDriveServiceWrapper;
    fake_drive_uploader_ = new FakeDriveUploader(fake_drive_service_);

    api_util_ = APIUtil::CreateForTesting(
        &profile_,
        scoped_ptr<DriveServiceInterface>(fake_drive_service_),
        scoped_ptr<DriveUploaderInterface>(fake_drive_uploader_));

    fake_drive_service_->LoadResourceListForWapi(
        "chromeos/sync_file_system/initialize.json");
  }

  virtual void TearDown() OVERRIDE {
    api_util_.reset();
  }

 protected:
  void SetUpSyncRootDirectory() {
    fake_drive_service()->AddNewDirectory(
        fake_drive_service_->GetRootResourceId(),
        APIUtil::GetSyncRootDirectoryName(),
        base::Bind(&DidAddNewDirectory, &sync_root_resource_id_));
    message_loop()->RunUntilIdle();

    ASSERT_TRUE(!sync_root_resource_id_.empty());
    fake_drive_service()->RemoveResourceFromDirectory(
        fake_drive_service_->GetRootResourceId(),
        sync_root_resource_id_,
        base::Bind(&DidRemoveResourceFromDirectory));
    message_loop()->RunUntilIdle();
  }

  void SetUpOriginRootDirectory() {
    ASSERT_TRUE(!sync_root_resource_id_.empty());
    fake_drive_service()->AddNewDirectory(
        GetSyncRootResourceId(),
        kOriginDirectoryName,
        base::Bind(&DidAddNewDirectory, &origin_root_resource_id_));
    message_loop()->RunUntilIdle();
  }

  void SetUpFile(const std::string& content_data,
                 const std::string& title,
                 std::string* resource_id_out,
                 std::string* file_md5_out) {
    ASSERT_TRUE(!origin_root_resource_id_.empty());
    ASSERT_TRUE(resource_id_out);
    ASSERT_TRUE(file_md5_out);
    fake_drive_service()->AddNewFile(
        "text/plain",
        content_data,
        origin_root_resource_id_,
        title,
        false,  // shared_with_me
        base::Bind(&DidAddNewFile, resource_id_out, file_md5_out));
    message_loop()->RunUntilIdle();
  }

  std::string GetSyncRootResourceId() {
    DCHECK(!sync_root_resource_id_.empty());
    return sync_root_resource_id_;
  }

  std::string GetOriginRootResourceId() {
    DCHECK(!origin_root_resource_id_.empty());
    return origin_root_resource_id_;
  }

  APIUtil* api_util() { return api_util_.get(); }

  FakeDriveServiceWrapper* fake_drive_service() {
    return fake_drive_service_;
  }

  FakeDriveUploader* fake_drive_uploader() {
    return fake_drive_uploader_;
  }

  base::MessageLoop* message_loop() { return &message_loop_; }

  void TestGetSyncRoot();
  void TestCreateSyncRoot();
  void TestCreateSyncRoot_Conflict();
  void TestGetOriginDirectory();
  void TestCreateOriginDirectory();
  void TestCreateOriginDirectory_Conflict();
  void TestGetLargestChangeStamp();
  void TestListFiles();
  void TestListChanges();
  void TestDownloadFile();
  void TestDownloadFileInNotModified();
  void TestUploadNewFile();
  void TestUploadNewFile_ConflictWithFile();
  void TestUploadExistingFile();
  void TestUploadExistingFileInConflict();
  void TestDeleteFile();
  void TestDeleteFileInConflict();
  void TestCreateDirectory();

 private:
  base::MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;

  std::string sync_root_resource_id_;
  std::string origin_root_resource_id_;

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

void APIUtilTest::TestGetSyncRoot() {
  fake_drive_service()->LoadAccountMetadataForWapi(
      "chromeos/sync_file_system/account_metadata.json");
  SetUpSyncRootDirectory();

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  api_util()->GetDriveDirectoryForSyncRoot(
      base::Bind(&DidGetResourceID, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_EQ(GetSyncRootResourceId(), resource_id);
}

void APIUtilTest::TestCreateSyncRoot() {
  fake_drive_service()->LoadAccountMetadataForWapi(
      "chromeos/sync_file_system/account_metadata.json");
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

void APIUtilTest::TestCreateSyncRoot_Conflict() {
  fake_drive_service()->LoadAccountMetadataForWapi(
      "chromeos/sync_file_system/account_metadata.json");
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

void APIUtilTest::TestGetOriginDirectory() {
  SetUpSyncRootDirectory();
  SetUpOriginRootDirectory();

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  api_util()->GetDriveDirectoryForOrigin(
      GetSyncRootResourceId(),
      GURL(kOrigin),
      base::Bind(&DidGetResourceID, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_EQ(GetOriginRootResourceId(), resource_id);
}

void APIUtilTest::TestCreateOriginDirectory() {
  SetUpSyncRootDirectory();

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  api_util()->GetDriveDirectoryForOrigin(
      GetSyncRootResourceId(),
      GURL(kOrigin),
      base::Bind(&DidGetResourceID, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_CREATED, error);
  EXPECT_FALSE(resource_id.empty());

  fake_drive_service()->SearchByTitle(
      kOriginDirectoryName,
      GetSyncRootResourceId(),
      base::Bind(&VerifyTitleUniqueness,
                 FROM_HERE,
                 resource_id,
                 google_apis::ENTRY_KIND_FOLDER));
  message_loop()->RunUntilIdle();
}

void APIUtilTest::TestCreateOriginDirectory_Conflict() {
  SetUpSyncRootDirectory();
  fake_drive_service()->set_make_directory_conflict(true);

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  api_util()->GetDriveDirectoryForOrigin(
      GetSyncRootResourceId(),
      GURL(kOrigin),
      base::Bind(&DidGetResourceID, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_FALSE(resource_id.empty());

  // Verify that there is no duplicated directory on the remote side.
  fake_drive_service()->SearchByTitle(
      kOriginDirectoryName,
      GetSyncRootResourceId(),
      base::Bind(&VerifyTitleUniqueness,
                 FROM_HERE,
                 resource_id,
                 google_apis::ENTRY_KIND_FOLDER));
  message_loop()->RunUntilIdle();
}

void APIUtilTest::TestGetLargestChangeStamp() {
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

void APIUtilTest::TestListFiles() {
  fake_drive_service()->set_default_max_results(3);

  SetUpSyncRootDirectory();
  SetUpOriginRootDirectory();

  int kNumberOfFiles = 5;
  for (int i = 0; i < kNumberOfFiles; ++i) {
    std::string file_resource_id;
    std::string file_md5;
    std::string file_content = base::StringPrintf("test content %d", i);
    std::string file_title = base::StringPrintf("test_%d.txt", i);
    SetUpFile(file_content, file_title, &file_resource_id, &file_md5);
  }

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> document_feed;
  api_util()->ListFiles(
      GetOriginRootResourceId(),
      base::Bind(&DidGetResourceList, &done, &error, &document_feed));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_EQ(3U, document_feed->entries().size());

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
  EXPECT_EQ(2U, document_feed->entries().size());
}

void APIUtilTest::TestListChanges() {
  const int64 kStartChangestamp = 6;

  SetUpSyncRootDirectory();
  SetUpOriginRootDirectory();

  // Files should have changestamp #4+ since creating the sync root directory is
  // #1, moving it out of 'My Drive' is #2, and creating the origin root
  // directory is #3.
  const int kNumberOfFiles = 5;
  for (int i = 0; i < kNumberOfFiles; ++i) {
    std::string file_resource_id;
    std::string file_md5;
    std::string file_content = base::StringPrintf("test content %d", i);
    std::string file_title = base::StringPrintf("test_%d.txt", i);
    SetUpFile(file_content, file_title, &file_resource_id, &file_md5);
  }

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> document_feed;
  api_util()->ListFiles(
      GetOriginRootResourceId(),
      base::Bind(&DidGetResourceList, &done, &error, &document_feed));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_EQ(5U, document_feed->entries().size());

  done = false;
  error = google_apis::GDATA_OTHER_ERROR;
  document_feed.reset();
  api_util()->ListChanges(
      kStartChangestamp,
      base::Bind(&DidGetResourceList, &done, &error, &document_feed));
  message_loop()->RunUntilIdle();

  // There should be 3 files which have changestamp #6+.
  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_EQ(3U, document_feed->entries().size());
}

void APIUtilTest::TestDownloadFile() {
  const std::string kFileContent = "test content";
  const std::string kFileTitle = "test.txt";

  SetUpSyncRootDirectory();
  SetUpOriginRootDirectory();
  std::string file_resource_id;
  std::string file_md5;
  SetUpFile(kFileContent, kFileTitle, &file_resource_id, &file_md5);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath kOutputFilePath =
      temp_dir.path().AppendASCII(kFileTitle);

  bool done = false;
  std::string downloaded_file_md5;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  api_util()->DownloadFile(
      file_resource_id,
      "",  // local_file_md5
      kOutputFilePath,
      base::Bind(&DidDownloadFile, &done, &downloaded_file_md5, &error));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(file_md5, downloaded_file_md5);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
}

void APIUtilTest::TestDownloadFileInNotModified() {
  const std::string kFileContent = "test content";
  const std::string kFileTitle = "test.txt";

  SetUpSyncRootDirectory();
  SetUpOriginRootDirectory();
  std::string file_resource_id;
  std::string file_md5;
  SetUpFile(kFileContent, kFileTitle, &file_resource_id, &file_md5);

  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
  const base::FilePath kOutputFilePath =
      temp_dir.path().AppendASCII(kFileTitle);

  bool done = false;
  std::string downloaded_file_md5;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;

  // Since local file's hash value is equal to remote file's one, it is expected
  // to cancel download the file and to return NOT_MODIFIED status code.
  api_util()->DownloadFile(
      file_resource_id,
      file_md5,
      kOutputFilePath,
      base::Bind(&DidDownloadFile, &done, &downloaded_file_md5, &error));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_NOT_MODIFIED, error);
  // TODO(nhiroki): Compare |file_md5| and |downloaded_file_md5| after making
  // FakeDriveService::AddNewEntry set the correct MD5.
}

void APIUtilTest::TestUploadNewFile() {
  const std::string kFileTitle = "test.txt";
  const base::FilePath kLocalFilePath(FPL("/tmp/dir/file"));

  SetUpSyncRootDirectory();
  SetUpOriginRootDirectory();

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  api_util()->UploadNewFile(
      GetOriginRootResourceId(),
      kLocalFilePath,
      kFileTitle,
      base::Bind(&DidUploadFile, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_CREATED, error);
  EXPECT_TRUE(!resource_id.empty());

  fake_drive_service()->SearchByTitle(
      kFileTitle,
      GetOriginRootResourceId(),
      base::Bind(&VerifyTitleUniqueness,
                 FROM_HERE,
                 resource_id,
                 google_apis::ENTRY_KIND_FILE));
  message_loop()->RunUntilIdle();
}

void APIUtilTest::TestUploadNewFile_ConflictWithFile() {
  const std::string kFileTitle = "test.txt";
  const base::FilePath kLocalFilePath(FPL("/tmp/dir/file"));

  fake_drive_uploader()->set_make_file_conflict(true);

  SetUpSyncRootDirectory();
  SetUpOriginRootDirectory();

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  api_util()->UploadNewFile(
      GetOriginRootResourceId(),
      kLocalFilePath,
      kFileTitle,
      base::Bind(&DidUploadFile, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  // HTTP_CONFLICT error must be returned with empty resource_id.
  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_CONFLICT, error);
  EXPECT_TRUE(!resource_id.empty());

  // Verify that there is no duplicated file on the remote side.
  fake_drive_service()->SearchByTitle(
      kFileTitle,
      GetOriginRootResourceId(),
      base::Bind(&VerifyTitleUniqueness,
                 FROM_HERE,
                 resource_id,
                 google_apis::ENTRY_KIND_FILE));
  message_loop()->RunUntilIdle();
}

void APIUtilTest::TestUploadExistingFile() {
  const base::FilePath kLocalFilePath(FPL("/tmp/dir/file"));
  const std::string kFileContent = "test content";
  const std::string kFileTitle = "test.txt";

  SetUpSyncRootDirectory();
  SetUpOriginRootDirectory();
  std::string file_resource_id;
  std::string file_md5;
  SetUpFile(kFileContent, kFileTitle, &file_resource_id, &file_md5);

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  api_util()->UploadExistingFile(
      file_resource_id,
      file_md5,
      kLocalFilePath,
      base::Bind(&DidUploadFile, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_EQ(file_resource_id, resource_id);

  fake_drive_service()->SearchByTitle(
      kFileTitle,
      GetOriginRootResourceId(),
      base::Bind(&VerifyTitleUniqueness,
                 FROM_HERE,
                 file_resource_id,
                 google_apis::ENTRY_KIND_FILE));
  message_loop()->RunUntilIdle();
}

void APIUtilTest::TestUploadExistingFileInConflict() {
  const base::FilePath kLocalFilePath(FPL("/tmp/dir/file"));
  const std::string kFileContent = "test content";
  const std::string kFileTitle = "test.txt";

  SetUpSyncRootDirectory();
  SetUpOriginRootDirectory();
  std::string file_resource_id;
  std::string file_md5;
  SetUpFile(kFileContent, kFileTitle, &file_resource_id, &file_md5);

  // Since remote file's hash value is different from the expected one, it is
  // expected to cancel upload the file and to return CONFLICT status code.
  const std::string kExpectedRemoteFileMD5 = "123456";

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  api_util()->UploadExistingFile(
      file_resource_id,
      kExpectedRemoteFileMD5,
      kLocalFilePath,
      base::Bind(&DidUploadFile, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_CONFLICT, error);
  EXPECT_TRUE(resource_id.empty());

  // Verify that there is no duplicated file on the remote side.
  fake_drive_service()->SearchByTitle(
      kFileTitle,
      GetOriginRootResourceId(),
      base::Bind(&VerifyTitleUniqueness,
                 FROM_HERE,
                 file_resource_id,
                 google_apis::ENTRY_KIND_FILE));
  message_loop()->RunUntilIdle();
}

void APIUtilTest::TestDeleteFile() {
  const std::string kFileContent = "test content";
  const std::string kFileTitle = "test.txt";

  SetUpSyncRootDirectory();
  SetUpOriginRootDirectory();
  std::string file_resource_id;
  std::string file_md5;
  SetUpFile(kFileContent, kFileTitle, &file_resource_id, &file_md5);

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  api_util()->DeleteFile(file_resource_id,
                         file_md5,
                         base::Bind(&DidDeleteFile, &done, &error));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);

  fake_drive_service()->SearchByTitle(
      kFileTitle,
      GetOriginRootResourceId(),
      base::Bind(&VerifyFileDeletion, FROM_HERE));
  message_loop()->RunUntilIdle();
}

void APIUtilTest::TestDeleteFileInConflict() {
  const std::string kFileContent = "test content";
  const std::string kFileTitle = "test.txt";

  SetUpSyncRootDirectory();
  SetUpOriginRootDirectory();
  std::string file_resource_id;
  std::string file_md5;
  SetUpFile(kFileContent, kFileTitle, &file_resource_id, &file_md5);

  // Since remote file's hash value is different from the expected one, it is
  // expected to cancel delete the file and to return CONFLICT status code.
  const std::string kExpectedRemoteFileMD5 = "123456";

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  api_util()->DeleteFile(file_resource_id,
                         kExpectedRemoteFileMD5,
                         base::Bind(&DidDeleteFile, &done, &error));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_CONFLICT, error);

  // Verify that the conflict file was not deleted on the remote side.
  fake_drive_service()->SearchByTitle(
      kFileTitle,
      GetOriginRootResourceId(),
      base::Bind(&VerifyTitleUniqueness,
                 FROM_HERE,
                 file_resource_id,
                 google_apis::ENTRY_KIND_FILE));
  message_loop()->RunUntilIdle();
}

void APIUtilTest::TestCreateDirectory() {
  const std::string kDirectoryTitle("directory");

  SetUpSyncRootDirectory();
  SetUpOriginRootDirectory();

  bool done = false;
  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  std::string resource_id;
  api_util()->CreateDirectory(
      GetOriginRootResourceId(),
      kDirectoryTitle,
      base::Bind(&DidGetResourceID, &done, &error, &resource_id));
  message_loop()->RunUntilIdle();

  EXPECT_TRUE(done);
  EXPECT_EQ(google_apis::HTTP_CREATED, error);
  EXPECT_FALSE(resource_id.empty());

  fake_drive_service()->SearchByTitle(
      kDirectoryTitle,
      GetOriginRootResourceId(),
      base::Bind(&VerifyTitleUniqueness,
                 FROM_HERE,
                 resource_id,
                 google_apis::ENTRY_KIND_FOLDER));
  message_loop()->RunUntilIdle();
}

TEST_F(APIUtilTest, GetSyncRoot) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestGetSyncRoot();
}

TEST_F(APIUtilTest, GetSyncRoot_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestGetSyncRoot();
}

TEST_F(APIUtilTest, CreateSyncRoot) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestCreateSyncRoot();
}

TEST_F(APIUtilTest, CreateSyncRoot_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestCreateSyncRoot();
}

TEST_F(APIUtilTest, CreateSyncRoot_Conflict) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestCreateSyncRoot_Conflict();
}

TEST_F(APIUtilTest, CreateSyncRoot_Conflict_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestCreateSyncRoot_Conflict();
}

TEST_F(APIUtilTest, GetOriginDirectory) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestGetOriginDirectory();
}

TEST_F(APIUtilTest, GetOriginDirectory_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestGetOriginDirectory();
}

TEST_F(APIUtilTest, CreateOriginDirectory) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestCreateOriginDirectory();
}

TEST_F(APIUtilTest, CreateOriginDirectory_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestCreateOriginDirectory();
}

TEST_F(APIUtilTest, CreateOriginDirectory_Conflict) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestCreateOriginDirectory_Conflict();
}

TEST_F(APIUtilTest, CreateOriginDirectory_Conflict_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestCreateOriginDirectory_Conflict();
}

TEST_F(APIUtilTest, GetLargestChangeStamp) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestGetLargestChangeStamp();
}

TEST_F(APIUtilTest, GetLargestChangeStamp_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestGetLargestChangeStamp();
}

TEST_F(APIUtilTest, ListFiles) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestListFiles();
}

TEST_F(APIUtilTest, ListFiles_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestListFiles();
}

TEST_F(APIUtilTest, ListChanges) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestListChanges();
}

TEST_F(APIUtilTest, ListChanges_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestListChanges();
}

TEST_F(APIUtilTest, DownloadFile) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestDownloadFile();
}

TEST_F(APIUtilTest, DownloadFile_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestDownloadFile();
}

TEST_F(APIUtilTest, DownloadFileInNotModified) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestDownloadFileInNotModified();
}

TEST_F(APIUtilTest, DownloadFileInNotModified_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestDownloadFileInNotModified();
}

TEST_F(APIUtilTest, UploadNewFile) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestUploadNewFile();
}

TEST_F(APIUtilTest, UploadNewFile_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestUploadNewFile();
}

TEST_F(APIUtilTest, UploadNewFile_ConflictWithFile) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestUploadNewFile_ConflictWithFile();
}

TEST_F(APIUtilTest, UploadNewFile_ConflictWithFile_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestUploadNewFile_ConflictWithFile();
}

TEST_F(APIUtilTest, UploadExistingFile) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestUploadExistingFile();
}

TEST_F(APIUtilTest, UploadExistingFile_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestUploadExistingFile();
}

TEST_F(APIUtilTest, UploadExistingFileInConflict) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestUploadExistingFileInConflict();
}

TEST_F(APIUtilTest, UploadExistingFileInConflict_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestUploadExistingFileInConflict();
}

TEST_F(APIUtilTest, DeleteFile) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestDeleteFile();
}

TEST_F(APIUtilTest, DeleteFile_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestDeleteFile();
}

TEST_F(APIUtilTest, DeleteFileInConflict) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestDeleteFileInConflict();
}

TEST_F(APIUtilTest, DeleteFileInConflict_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestDeleteFileInConflict();
}

TEST_F(APIUtilTest, CreateDirectory) {
  ASSERT_FALSE(IsDriveAPIDisabled());
  TestCreateDirectory();
}

TEST_F(APIUtilTest, CreateDirectory_WAPI) {
  ScopedDisableDriveAPI disable_drive_api;
  TestCreateDirectory();
}

#endif  // !defined(OS_ANDROID)

}  // namespace drive
}  // namespace sync_file_system
