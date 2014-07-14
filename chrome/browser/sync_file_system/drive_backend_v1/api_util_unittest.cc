// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync_file_system/drive_backend_v1/api_util.h"

#include "base/file_util.h"
#include "base/location.h"
#include "base/message_loop/message_loop.h"
#include "base/strings/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/drive/drive_uploader.h"
#include "chrome/browser/drive/fake_drive_service.h"
#include "chrome/browser/sync_file_system/drive_backend/fake_drive_service_helper.h"
#include "chrome/browser/sync_file_system/drive_backend/fake_drive_uploader.h"
#include "chrome/browser/sync_file_system/drive_backend_v1/drive_file_sync_util.h"
#include "content/public/test/test_browser_thread.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/gdata_errorcode.h"
#include "google_apis/drive/test_util.h"
#include "net/base/escape.h"
#include "testing/gtest/include/gtest/gtest.h"

#define FPL(x) FILE_PATH_LITERAL(x)

using drive::DriveServiceInterface;
using drive::DriveUploaderInterface;
using google_apis::FileResource;
using google_apis::GDataErrorCode;
using google_apis::ResourceEntry;
using google_apis::ResourceList;

namespace sync_file_system {
namespace drive_backend {

namespace {

const char kOrigin[] = "chrome-extension://example";
const char kOriginDirectoryName[] = "example";

struct Output {
  GDataErrorCode error;
  std::string resource_id;
  std::string file_md5;
  int64 largest_changestamp;

  Output() : error(google_apis::GDATA_OTHER_ERROR),
             largest_changestamp(-1) {
  }
};

}  // namespace

class APIUtilTest : public testing::Test {
 public:
  APIUtilTest() : thread_bundle_(content::TestBrowserThreadBundle::IO_MAINLOOP),
                  fake_drive_service_(NULL),
                  fake_drive_uploader_(NULL) {}

  virtual void SetUp() OVERRIDE {
    fake_drive_service_ = new FakeDriveServiceWrapper;
    fake_drive_uploader_ = new FakeDriveUploader(fake_drive_service_);

    fake_drive_helper_.reset(new FakeDriveServiceHelper(
        fake_drive_service_, fake_drive_uploader_,
        APIUtil::GetSyncRootDirectoryName()));

    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    api_util_ = APIUtil::CreateForTesting(
        temp_dir_.path(),
        scoped_ptr<DriveServiceInterface>(fake_drive_service_),
        scoped_ptr<DriveUploaderInterface>(fake_drive_uploader_));
  }

  virtual void TearDown() OVERRIDE {
    api_util_.reset();
  }

 protected:
  std::string SetUpSyncRootDirectory() {
    std::string sync_root_id;
    EXPECT_EQ(google_apis::HTTP_CREATED,
              fake_drive_helper_->AddOrphanedFolder(
                  APIUtil::GetSyncRootDirectoryName(),
                  &sync_root_id));
    return sync_root_id;
  }

  std::string SetUpOriginRootDirectory(const std::string& sync_root_id) {
    std::string origin_root_id;
    EXPECT_EQ(google_apis::HTTP_CREATED,
              fake_drive_helper_->AddFolder(
                  sync_root_id,
                  kOriginDirectoryName,
                  &origin_root_id));
    return origin_root_id;
  }

  void SetUpFile(const std::string& origin_root_id,
                 const std::string& content_data,
                 const std::string& title,
                 scoped_ptr<FileResource>* entry) {
    ASSERT_TRUE(entry);
    std::string file_resource_id;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->AddFile(
                  origin_root_id,
                  title,
                  content_data,
                  &file_resource_id));
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->GetFileResource(
                  file_resource_id,
                  entry));
  }

  void VerifyTitleUniqueness(
      const std::string& parent_resource_id,
      const std::string& title,
      const std::string& resource_id,
      google_apis::ResourceEntry::ResourceEntryKind kind) {
    ScopedVector<ResourceEntry> entries;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->SearchByTitle(
                  parent_resource_id, title, &entries));
    ASSERT_EQ(1u, entries.size());
    EXPECT_EQ(resource_id, entries[0]->resource_id());
    EXPECT_EQ(kind, entries[0]->kind());
  }

  void VerifyFileDeletion(const std::string& parent_resource_id,
                          const std::string& title) {
    ScopedVector<ResourceEntry> entries;
    EXPECT_EQ(google_apis::HTTP_SUCCESS,
              fake_drive_helper_->SearchByTitle(
                  parent_resource_id, title, &entries));
    EXPECT_TRUE(entries.empty());
  }

  APIUtil* api_util() { return api_util_.get(); }

  FakeDriveServiceWrapper* fake_drive_service() {
    return fake_drive_service_;
  }

  FakeDriveUploader* fake_drive_uploader() {
    return fake_drive_uploader_;
  }

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
  content::TestBrowserThreadBundle thread_bundle_;

  base::ScopedTempDir temp_dir_;
  scoped_ptr<APIUtil> api_util_;
  FakeDriveServiceWrapper* fake_drive_service_;
  FakeDriveUploader* fake_drive_uploader_;
  scoped_ptr<FakeDriveServiceHelper> fake_drive_helper_;

  DISALLOW_COPY_AND_ASSIGN(APIUtilTest);
};

void DidGetResourceID(Output* output,
                      GDataErrorCode error,
                      const std::string& resource_id) {
  ASSERT_TRUE(output);
  output->error = error;
  output->resource_id = resource_id;
}

void DidGetLargestChangeStamp(Output* output,
                              GDataErrorCode error,
                              int64 largest_changestamp) {
  ASSERT_TRUE(output);
  output->error = error;
  output->largest_changestamp = largest_changestamp;
}

void DidGetResourceList(GDataErrorCode* error_out,
                        scoped_ptr<ResourceList>* document_feed_out,
                        GDataErrorCode error,
                        scoped_ptr<ResourceList> document_feed) {
  ASSERT_TRUE(error_out);
  ASSERT_TRUE(document_feed_out);
  *error_out = error;
  *document_feed_out = document_feed.Pass();
}

void DidDownloadFile(Output* output,
                     GDataErrorCode error,
                     const std::string& file_md5,
                     int64 file_size,
                     const base::Time& updated_time,
                     webkit_blob::ScopedFile file) {
  ASSERT_TRUE(output);
  ASSERT_TRUE(base::PathExists(file.path()));
  output->error = error;
  output->file_md5 = file_md5;
}

void DidUploadFile(Output* output,
                   GDataErrorCode error,
                   const std::string& resource_id,
                   const std::string& file_md5) {
  ASSERT_TRUE(output);
  output->error = error;
  output->resource_id = resource_id;
  output->file_md5 = file_md5;
}

void DidDeleteFile(GDataErrorCode* error_out,
                   GDataErrorCode error) {
  ASSERT_TRUE(error);
  *error_out = error;
}

void APIUtilTest::TestGetSyncRoot() {
  const std::string sync_root_id = SetUpSyncRootDirectory();

  Output output;
  api_util()->GetDriveDirectoryForSyncRoot(
      base::Bind(&DidGetResourceID, &output));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(google_apis::HTTP_SUCCESS, output.error);
  EXPECT_EQ(sync_root_id, output.resource_id);
}

void APIUtilTest::TestCreateSyncRoot() {
  Output output;
  api_util()->GetDriveDirectoryForSyncRoot(
      base::Bind(&DidGetResourceID, &output));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(google_apis::HTTP_CREATED, output.error);
  EXPECT_FALSE(output.resource_id.empty());

  VerifyTitleUniqueness(std::string(),  // directory_resource_id
                        APIUtil::GetSyncRootDirectoryName(),
                        output.resource_id,
                        google_apis::ResourceEntry::ENTRY_KIND_FOLDER);
}

void APIUtilTest::TestCreateSyncRoot_Conflict() {
  fake_drive_service()->set_make_directory_conflict(true);

  Output output;
  api_util()->GetDriveDirectoryForSyncRoot(
      base::Bind(&DidGetResourceID, &output));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(google_apis::HTTP_SUCCESS, output.error);
  EXPECT_FALSE(output.resource_id.empty());

  // Verify that there is no duplicated directory on the remote side.
  VerifyTitleUniqueness(std::string(),  // directory_resource_id
                        APIUtil::GetSyncRootDirectoryName(),
                        output.resource_id,
                        google_apis::ResourceEntry::ENTRY_KIND_FOLDER);
}

void APIUtilTest::TestGetOriginDirectory() {
  const std::string sync_root_id = SetUpSyncRootDirectory();
  const std::string origin_root_id = SetUpOriginRootDirectory(sync_root_id);

  Output output;
  api_util()->GetDriveDirectoryForOrigin(
      sync_root_id,
      GURL(kOrigin),
      base::Bind(&DidGetResourceID, &output));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(google_apis::HTTP_SUCCESS, output.error);
  EXPECT_EQ(origin_root_id, output.resource_id);
}

void APIUtilTest::TestCreateOriginDirectory() {
  const std::string& sync_root_id = SetUpSyncRootDirectory();

  Output output;
  api_util()->GetDriveDirectoryForOrigin(
      sync_root_id,
      GURL(kOrigin),
      base::Bind(&DidGetResourceID, &output));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(google_apis::HTTP_CREATED, output.error);
  EXPECT_FALSE(output.resource_id.empty());

  VerifyTitleUniqueness(sync_root_id,
                        kOriginDirectoryName,
                        output.resource_id,
                        google_apis::ResourceEntry::ENTRY_KIND_FOLDER);
}

void APIUtilTest::TestCreateOriginDirectory_Conflict() {
  fake_drive_service()->set_make_directory_conflict(true);
  const std::string sync_root_id = SetUpSyncRootDirectory();

  Output output;
  api_util()->GetDriveDirectoryForOrigin(
      sync_root_id,
      GURL(kOrigin),
      base::Bind(&DidGetResourceID, &output));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(google_apis::HTTP_SUCCESS, output.error);
  EXPECT_FALSE(output.resource_id.empty());

  // Verify that there is no duplicated directory on the remote side.
  VerifyTitleUniqueness(sync_root_id,
                        kOriginDirectoryName,
                        output.resource_id,
                        google_apis::ResourceEntry::ENTRY_KIND_FOLDER);
}

void APIUtilTest::TestGetLargestChangeStamp() {
  Output output;
  api_util()->GetLargestChangeStamp(
      base::Bind(&DidGetLargestChangeStamp, &output));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(google_apis::HTTP_SUCCESS, output.error);
  EXPECT_EQ(fake_drive_service()->about_resource().largest_change_id(),
            output.largest_changestamp);
}

void APIUtilTest::TestListFiles() {
  fake_drive_service()->set_default_max_results(3);
  const std::string sync_root_id = SetUpSyncRootDirectory();
  const std::string origin_root_id = SetUpOriginRootDirectory(sync_root_id);

  int kNumberOfFiles = 5;
  for (int i = 0; i < kNumberOfFiles; ++i) {
    scoped_ptr<FileResource> file;
    std::string file_content = base::StringPrintf("test content %d", i);
    std::string file_title = base::StringPrintf("test_%d.txt", i);
    SetUpFile(origin_root_id, file_content, file_title, &file);
  }

  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> document_feed;
  api_util()->ListFiles(
      origin_root_id,
      base::Bind(&DidGetResourceList, &error, &document_feed));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_EQ(3U, document_feed->entries().size());

  GURL feed_url;
  ASSERT_TRUE(document_feed->GetNextFeedURL(&feed_url));

  error = google_apis::GDATA_OTHER_ERROR;
  document_feed.reset();

  api_util()->ContinueListing(
      feed_url,
      base::Bind(&DidGetResourceList, &error, &document_feed));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_EQ(2U, document_feed->entries().size());
}

void APIUtilTest::TestListChanges() {
  const int64 old_changestamp =
      fake_drive_service()->about_resource().largest_change_id();
  const std::string sync_root_id = SetUpSyncRootDirectory();
  const std::string origin_root_id = SetUpOriginRootDirectory(sync_root_id);

  // Files should have changestamp #4+ since creating the sync root directory is
  // #1, moving it out of 'My Drive' is #2, and creating the origin root
  // directory is #3.
  const int kNumberOfFiles = 5;
  for (int i = 0; i < kNumberOfFiles; ++i) {
    scoped_ptr<FileResource> file;
    std::string file_content = base::StringPrintf("test content %d", i);
    std::string file_title = base::StringPrintf("test_%d.txt", i);
    SetUpFile(origin_root_id, file_content, file_title, &file);
  }

  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  scoped_ptr<ResourceList> document_feed;
  api_util()->ListFiles(
      origin_root_id,
      base::Bind(&DidGetResourceList, &error, &document_feed));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_EQ(5U, document_feed->entries().size());

  error = google_apis::GDATA_OTHER_ERROR;
  document_feed.reset();
  api_util()->ListChanges(
      old_changestamp + 6,
      base::Bind(&DidGetResourceList, &error, &document_feed));
  base::MessageLoop::current()->RunUntilIdle();

  // There should be 3 files which have changestamp #6+.
  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);
  EXPECT_EQ(3U, document_feed->entries().size());
}

void APIUtilTest::TestDownloadFile() {
  const std::string kFileContent = "test content";
  const std::string kFileTitle = "test.txt";
  const std::string sync_root_id = SetUpSyncRootDirectory();
  const std::string origin_root_id = SetUpOriginRootDirectory(sync_root_id);

  scoped_ptr<FileResource> file;
  SetUpFile(origin_root_id, kFileContent, kFileTitle, &file);

  Output output;
  api_util()->DownloadFile(
      file->file_id(),
      "",  // local_file_md5
      base::Bind(&DidDownloadFile, &output));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(file->md5_checksum(), output.file_md5);
  EXPECT_EQ(google_apis::HTTP_SUCCESS, output.error);
}

void APIUtilTest::TestDownloadFileInNotModified() {
  const std::string kFileContent = "test content";
  const std::string kFileTitle = "test.txt";
  const std::string sync_root_id = SetUpSyncRootDirectory();
  const std::string origin_root_id = SetUpOriginRootDirectory(sync_root_id);

  scoped_ptr<FileResource> file;
  SetUpFile(origin_root_id, kFileContent, kFileTitle, &file);

  // Since local file's hash value is equal to remote file's one, it is expected
  // to cancel download the file and to return NOT_MODIFIED status code.
  Output output;
  api_util()->DownloadFile(
      file->file_id(),
      file->md5_checksum(),
      base::Bind(&DidDownloadFile, &output));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(file->md5_checksum(), output.file_md5);
  EXPECT_EQ(google_apis::HTTP_NOT_MODIFIED, output.error);
}

void APIUtilTest::TestUploadNewFile() {
  const std::string kFileTitle = "test.txt";
  const base::FilePath kLocalFilePath(FPL("/tmp/dir/file"));
  const std::string sync_root_id = SetUpSyncRootDirectory();
  const std::string origin_root_id = SetUpOriginRootDirectory(sync_root_id);

  Output output;
  api_util()->UploadNewFile(
      origin_root_id,
      kLocalFilePath,
      kFileTitle,
      base::Bind(&DidUploadFile, &output));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(google_apis::HTTP_CREATED, output.error);
  EXPECT_TRUE(!output.resource_id.empty());

  VerifyTitleUniqueness(origin_root_id,
                        kFileTitle,
                        output.resource_id,
                        google_apis::ResourceEntry::ENTRY_KIND_FILE);
}

void APIUtilTest::TestUploadNewFile_ConflictWithFile() {
  const std::string kFileTitle = "test.txt";
  const base::FilePath kLocalFilePath(FPL("/tmp/dir/file"));
  const std::string sync_root_id = SetUpSyncRootDirectory();
  const std::string origin_root_id = SetUpOriginRootDirectory(sync_root_id);

  fake_drive_uploader()->set_make_file_conflict(true);

  Output output;
  api_util()->UploadNewFile(
      origin_root_id,
      kLocalFilePath,
      kFileTitle,
      base::Bind(&DidUploadFile, &output));
  base::MessageLoop::current()->RunUntilIdle();

  // HTTP_CONFLICT error must be returned with empty resource_id.
  EXPECT_EQ(google_apis::HTTP_CONFLICT, output.error);
  EXPECT_TRUE(!output.resource_id.empty());

  // Verify that there is no duplicated file on the remote side.
  VerifyTitleUniqueness(origin_root_id,
                        kFileTitle,
                        output.resource_id,
                        google_apis::ResourceEntry::ENTRY_KIND_FILE);
}

void APIUtilTest::TestUploadExistingFile() {
  const base::FilePath kLocalFilePath(FPL("/tmp/dir/file"));
  const std::string kFileContent = "test content";
  const std::string kFileTitle = "test.txt";
  const std::string sync_root_id = SetUpSyncRootDirectory();
  const std::string origin_root_id = SetUpOriginRootDirectory(sync_root_id);

  scoped_ptr<FileResource> file;
  SetUpFile(origin_root_id, kFileContent, kFileTitle, &file);

  Output output;
  api_util()->UploadExistingFile(
      file->file_id(),
      file->md5_checksum(),
      kLocalFilePath,
      base::Bind(&DidUploadFile, &output));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(google_apis::HTTP_SUCCESS, output.error);
  EXPECT_EQ(file->file_id(), output.resource_id);

  VerifyTitleUniqueness(origin_root_id,
                        file->title(),
                        file->file_id(),
                        google_apis::ResourceEntry::ENTRY_KIND_FILE);
}

void APIUtilTest::TestUploadExistingFileInConflict() {
  const base::FilePath kLocalFilePath(FPL("/tmp/dir/file"));
  const std::string kFileContent = "test content";
  const std::string kFileTitle = "test.txt";
  const std::string sync_root_id = SetUpSyncRootDirectory();
  const std::string origin_root_id = SetUpOriginRootDirectory(sync_root_id);

  scoped_ptr<FileResource> file;
  SetUpFile(origin_root_id, kFileContent, kFileTitle, &file);

  // Since remote file's hash value is different from the expected one, it is
  // expected to cancel upload the file and to return CONFLICT status code.
  const std::string kExpectedRemoteFileMD5 = "123456";

  Output output;
  api_util()->UploadExistingFile(
      file->file_id(),
      kExpectedRemoteFileMD5,
      kLocalFilePath,
      base::Bind(&DidUploadFile, &output));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(google_apis::HTTP_CONFLICT, output.error);
  EXPECT_TRUE(output.resource_id.empty());

  // Verify that there is no duplicated file on the remote side.
  VerifyTitleUniqueness(origin_root_id,
                        file->title(),
                        file->file_id(),
                        google_apis::ResourceEntry::ENTRY_KIND_FILE);
}

void APIUtilTest::TestDeleteFile() {
  const std::string kFileContent = "test content";
  const std::string kFileTitle = "test.txt";
  const std::string sync_root_id = SetUpSyncRootDirectory();
  const std::string origin_root_id = SetUpOriginRootDirectory(sync_root_id);

  scoped_ptr<FileResource> file;
  SetUpFile(origin_root_id, kFileContent, kFileTitle, &file);

  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  api_util()->DeleteFile(file->file_id(),
                         file->md5_checksum(),
                         base::Bind(&DidDeleteFile, &error));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(google_apis::HTTP_SUCCESS, error);

  VerifyFileDeletion(origin_root_id, kFileTitle);
}

void APIUtilTest::TestDeleteFileInConflict() {
  const std::string kFileContent = "test content";
  const std::string kFileTitle = "test.txt";
  const std::string sync_root_id = SetUpSyncRootDirectory();
  const std::string origin_root_id = SetUpOriginRootDirectory(sync_root_id);

  scoped_ptr<FileResource> file;
  SetUpFile(origin_root_id, kFileContent, kFileTitle, &file);

  // Since remote file's hash value is different from the expected one, it is
  // expected to cancel delete the file and to return CONFLICT status code.
  const std::string kExpectedRemoteFileMD5 = "123456";

  GDataErrorCode error = google_apis::GDATA_OTHER_ERROR;
  api_util()->DeleteFile(file->file_id(),
                         kExpectedRemoteFileMD5,
                         base::Bind(&DidDeleteFile, &error));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(google_apis::HTTP_CONFLICT, error);

  // Verify that the conflict file was not deleted on the remote side.
  VerifyTitleUniqueness(origin_root_id,
                        file->title(),
                        file->file_id(),
                        google_apis::ResourceEntry::ENTRY_KIND_FILE);
}

void APIUtilTest::TestCreateDirectory() {
  const std::string kDirectoryTitle("directory");
  const std::string sync_root_id = SetUpSyncRootDirectory();
  const std::string origin_root_id = SetUpOriginRootDirectory(sync_root_id);

  Output output;
  api_util()->CreateDirectory(
      origin_root_id,
      kDirectoryTitle,
      base::Bind(&DidGetResourceID, &output));
  base::MessageLoop::current()->RunUntilIdle();

  EXPECT_EQ(google_apis::HTTP_CREATED, output.error);
  EXPECT_FALSE(output.resource_id.empty());

  VerifyTitleUniqueness(origin_root_id,
                        kDirectoryTitle,
                        output.resource_id,
                        google_apis::ResourceEntry::ENTRY_KIND_FOLDER);
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

}  // namespace drive_backend
}  // namespace sync_file_system
