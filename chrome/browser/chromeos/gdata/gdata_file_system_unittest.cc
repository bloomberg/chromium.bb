// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/threading/sequenced_worker_pool.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata_mock.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
#include "content/public/browser/browser_thread.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::AnyNumber;
using ::testing::Return;
using ::testing::ReturnNull;
using ::testing::_;

using base::Value;
using base::DictionaryValue;
using base::ListValue;

namespace gdata {

const char kSlash[] = "/";
const char kEscapedSlash[] = "\xE2\x88\x95";

class GDataFileSystemTestBase : public testing::Test {
 protected:
  GDataFileSystemTestBase()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_system_(NULL) {
  }

  virtual void Init(Profile* profile) {
    callback_helper_ = new CallbackHelper;

    // Allocate and keep a weak pointer to the mock, and inject it into the
    // GDataFileSystem object.
    mock_doc_service_ = new MockDocumentsService;

    EXPECT_CALL(*mock_doc_service_, Initialize(profile_.get())).Times(1);

    ASSERT_FALSE(file_system_);
    file_system_ = new GDataFileSystem(profile_.get(), mock_doc_service_);

    RunAllPendingForCache();
  }

  virtual void TearDown() OVERRIDE {
    ASSERT_TRUE(file_system_);
    EXPECT_CALL(*mock_doc_service_, CancelAll()).Times(1);
    file_system_->Shutdown();
    delete file_system_;
    file_system_ = NULL;
  }

  // Loads test json file as root ("/gdata") element.
  void LoadRootFeedDocument(const std::string& filename) {
    LoadSubdirFeedDocument(FilePath(FILE_PATH_LITERAL("gdata")), filename);
  }

  // Loads test json file as subdirectory content of |directory_path|.
  void LoadSubdirFeedDocument(const FilePath& directory_path,
                              const std::string& filename) {
    std::string error;
    scoped_ptr<Value> document(LoadJSONFile(filename));
    ASSERT_TRUE(document.get());
    ASSERT_TRUE(document->GetType() == Value::TYPE_DICTIONARY);
    GURL unused;
    scoped_ptr<ListValue> feed_list(new ListValue());
    feed_list->Append(document.release());
    ASSERT_TRUE(UpdateContent(directory_path, feed_list.get()));
  }

  void AddDirectoryFromFile(const FilePath& directory_path,
                            const std::string& filename) {
    std::string error;
    scoped_ptr<Value> atom(LoadJSONFile(filename));
    ASSERT_TRUE(atom.get());
    ASSERT_TRUE(atom->GetType() == Value::TYPE_DICTIONARY);

    DictionaryValue* dict_value = NULL;
    Value* entry_value = NULL;
    ASSERT_TRUE(atom->GetAsDictionary(&dict_value));
    ASSERT_TRUE(dict_value->Get("entry", &entry_value));

    DictionaryValue* entry_dict = NULL;
    ASSERT_TRUE(entry_value->GetAsDictionary(&entry_dict));

    // Tweak entry title to match the last segment of the directory path
    // (new directory name).
    std::vector<FilePath::StringType> dir_parts;
    directory_path.GetComponents(&dir_parts);
    entry_dict->SetString("title.$t", dir_parts[dir_parts.size() - 1]);

    ASSERT_EQ(file_system_->AddNewDirectory(directory_path, entry_value),
              base::PLATFORM_FILE_OK);
  }

  // Updates the content of directory under |directory_path| with parsed feed
  // |value|.
  bool UpdateContent(const FilePath& directory_path,
                     ListValue* list) {
    GURL unused;
    return file_system_->UpdateDirectoryWithDocumentFeed(
        directory_path,
        list) == base::PLATFORM_FILE_OK;
  }

  bool RemoveFile(const FilePath& file_path) {
    return file_system_->RemoveFileFromFileSystem(file_path) ==
        base::PLATFORM_FILE_OK;
  }

  GDataFileBase* FindFile(const FilePath& file_path) {
    scoped_refptr<ReadOnlyFindFileDelegate> search_delegate(
        new ReadOnlyFindFileDelegate());
    file_system_->FindFileByPath(file_path,
                                 search_delegate);
    return search_delegate->file();
  }

  void FindAndTestFilePath(const FilePath& file_path) {
    GDataFileBase* file = FindFile(file_path);
    ASSERT_TRUE(file);
    EXPECT_EQ(file->GetFilePath(), file_path);
  }

  GDataFileBase* FindFileByResource(const std::string& resource) {
    return file_system_->root_->GetFileByResource(resource);
  }

  void TestGetCacheFilePath(const std::string& res_id, const std::string& md5,
                            const std::string& expected_filename) {
    FilePath actual_path = file_system_->GetCacheFilePath(res_id, md5);
    FilePath expected_path =
        file_system_->cache_paths_[GDataFileSystem::CACHE_TYPE_BLOBS];
    expected_path = expected_path.Append(expected_filename);
    EXPECT_EQ(expected_path, actual_path);

    FilePath base_name = actual_path.BaseName();

    // FilePath::Extension returns ".", so strip it.
    std::string unescaped_md5 = GDataFileBase::UnescapeUtf8FileName(
        base_name.Extension().substr(1));
    EXPECT_EQ(md5, unescaped_md5);
    std::string unescaped_res_id = GDataFileBase::UnescapeUtf8FileName(
        base_name.RemoveExtension().value());
    EXPECT_EQ(res_id, unescaped_res_id);
  }

  void TestStoreToCache(const std::string& res_id, const std::string& md5,
                        const FilePath& source_path) {
    // Cache file for |res_id| and |md5| should not exist.
    EXPECT_FALSE(file_system_->root_->CacheFileExists(res_id, md5));

    // StoreToCache will move |source_path| to the cache dir; in order to keep
    // the test data file where it is, we need to copy |source_path| to
    // the profile dir and use that file for StoreToCache instead.
    FilePath temp_path = profile_->GetPath();
    temp_path = temp_path.Append(source_path.BaseName());
    EXPECT_TRUE(file_util::CopyFile(source_path, temp_path));

    EXPECT_TRUE(file_system_->StoreToCache(res_id, md5, temp_path,
                base::Bind(&GDataFileSystemTestBase::VerifyStoreToCache,
                            base::Unretained(this))));

    RunAllPendingForCache();
  }

  void VerifyStoreToCache(net::Error error, const std::string& res_id,
                          const std::string& md5) {
    EXPECT_EQ(net::OK, error);
    VerifyCacheFileStatusAndMode(res_id, md5, GDataFile::CACHE_STATE_PRESENT,
                                 S_IROTH);
  }

  void TestGetFromCache(const std::string& res_id, const std::string& md5) {
    FilePath path;
    EXPECT_TRUE(file_system_->GetFromCache(res_id, md5, &path));
    FilePath base_name = path.BaseName();
    EXPECT_EQ(res_id + FilePath::kExtensionSeparator + md5,
              base_name.value());
  }

  void TestRemoveFromCache(const std::string& res_id) {
    base::AutoLock lock(file_system_->lock_);
    EXPECT_TRUE(file_system_->RemoveFromCache(res_id,
                base::Bind(&GDataFileSystemTestBase::VerifyRemoveFromCache,
                            base::Unretained(this))));

    RunAllPendingForCache();
  }

  void VerifyRemoveFromCache(net::Error error, const std::string& res_id,
                             const std::string& md5) {
    EXPECT_EQ(net::OK, error);

    // Verify cache map.
    EXPECT_FALSE(file_system_->root_->CacheFileExists(res_id, md5));

    // Verify that no files with $res_id.* exist in dir.
    FilePath path = file_system_->GetCacheFilePath(res_id, "*");
    std::string all_res_id = res_id + FilePath::kExtensionSeparator + "*";
    file_util::FileEnumerator traversal(path.DirName(), false,
                                        file_util::FileEnumerator::FILES,
                                        all_res_id);
    EXPECT_TRUE(traversal.Next().empty());
  }

  void TestPin(const std::string& res_id, const std::string& md5) {
    EXPECT_TRUE(file_system_->Pin(res_id, md5,
                base::Bind(&GDataFileSystemTestBase::VerifyPin,
                           base::Unretained(this))));

    RunAllPendingForCache();
  }

  void VerifyPin(net::Error error, const std::string& res_id,
                 const std::string& md5) {
    EXPECT_EQ(net::OK, error);
    VerifyCacheFileStatusAndMode(res_id, md5,
                                 GDataFile::CACHE_STATE_PRESENT |
                                 GDataFile::CACHE_STATE_PINNED,
                                 S_IROTH | S_IXOTH);
  }

  void TestUnpin(const std::string& res_id, const std::string& md5) {
    EXPECT_TRUE(file_system_->Unpin(res_id, md5,
                base::Bind(&GDataFileSystemTestBase::VerifyUnpin,
                           base::Unretained(this))));

    RunAllPendingForCache();
  }

  void VerifyUnpin(net::Error error, const std::string& res_id,
                  const std::string& md5) {
    EXPECT_EQ(net::OK, error);
    VerifyCacheFileStatusAndMode(res_id, md5, GDataFile::CACHE_STATE_PRESENT,
                                 S_IROTH);
  }

  void VerifyCacheFileStatusAndMode(const std::string& res_id,
                                    const std::string& md5,
                                    int expected_status_flags,
                                    mode_t expected_mode_bits) {
    // Verify cache map.
    EXPECT_TRUE(file_system_->root_->CacheFileExists(res_id, md5));
    EXPECT_EQ(expected_status_flags,
              file_system_->root_->GetCacheState(res_id, md5));

    // Verify file attributes of actual cache file.
    FilePath path = file_system_->GetCacheFilePath(res_id, md5);
    struct stat64 stat_buf;
    EXPECT_EQ(0, stat64(path.value().c_str(), &stat_buf));
    EXPECT_TRUE(stat_buf.st_mode & expected_mode_bits);
  }

  void RunAllPendingForCache() {
    // Let cache operations run on IO blocking pool.
    content::BrowserThread::GetBlockingPool()->FlushForTesting();
    // Let callbacks for cache operations run on UI thread.
    message_loop_.RunAllPending();
  }

  static Value* LoadJSONFile(const std::string& filename) {
    FilePath path = GetTestFilePath(filename);

    std::string error;
    JSONFileValueSerializer serializer(path);
    Value* value = serializer.Deserialize(NULL, &error);
    EXPECT_TRUE(value) <<
        "Parse error " << path.value() << ": " << error;
    return value;
  }

  static FilePath GetTestFilePath(const std::string& filename) {
    FilePath path;
    std::string error;
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    path = path.AppendASCII("chromeos")
        .AppendASCII("gdata")
        .AppendASCII(filename.c_str());
    EXPECT_TRUE(file_util::PathExists(path)) <<
        "Couldn't find " << path.value();
    return path;
  }

  // This is used as a helper for registering callbacks that need to be
  // RefCountedThreadSafe, and a place where we can fetch results from various
  // operations.
  class CallbackHelper
    : public base::RefCountedThreadSafe<CallbackHelper> {
   public:
    CallbackHelper() : last_error_(base::PLATFORM_FILE_OK) {}
    virtual ~CallbackHelper() {}
    virtual void GetFileCallback(base::PlatformFileError error,
                                 const FilePath& file_path) {
      last_error_ = error;
      download_path_ = file_path;
    }
    virtual void FileOperationCallback(base::PlatformFileError error) {
      last_error_ = error;
    }

    base::PlatformFileError last_error_;
    FilePath download_path_;
  };

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<CallbackHelper> callback_helper_;
  GDataFileSystem* file_system_;
  MockDocumentsService* mock_doc_service_;
};

class GDataFileSystemTest : public GDataFileSystemTestBase {
 protected:
  GDataFileSystemTest() {
  }

  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile);
    Init(profile_.get());
  }
};

// Delegate used to find a directory element for file system updates.
class MockFindFileDelegate : public gdata::FindFileDelegate {
 public:
  MockFindFileDelegate() {
  }

  virtual ~MockFindFileDelegate() {
  }

  // gdata::FindFileDelegate overrides.
  MOCK_METHOD1(OnFileFound, void(GDataFile*));
  MOCK_METHOD2(OnDirectoryFound, void(const FilePath&, GDataDirectory* dir));
  MOCK_METHOD2(OnEnterDirectory, FindFileTraversalCommand(
      const FilePath&, GDataDirectory* dir));
  MOCK_METHOD1(OnError, void(base::PlatformFileError));
};

TEST_F(GDataFileSystemTest, SearchRootDirectory) {
  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate =
      new MockFindFileDelegate();

  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnDirectoryFound(FilePath(FILE_PATH_LITERAL("gdata")), _))
      .Times(1);

  file_system_->FindFileByPath(FilePath(FILE_PATH_LITERAL("gdata")),
                               mock_find_file_delegate);
}

TEST_F(GDataFileSystemTest, SearchExistingFile) {
  LoadRootFeedDocument("root_feed.json");
  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate =
      new MockFindFileDelegate();

  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath(FILE_PATH_LITERAL("gdata")), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));
  EXPECT_CALL(*mock_find_file_delegate.get(), OnFileFound(_))
      .Times(1);

  file_system_->FindFileByPath(FilePath(FILE_PATH_LITERAL("gdata/File 1.txt")),
                               mock_find_file_delegate);
}

TEST_F(GDataFileSystemTest, SearchEncodedFileNames) {
  LoadRootFeedDocument("root_feed.json");
  LoadSubdirFeedDocument(
      FilePath::FromUTF8Unsafe("gdata/Slash \xE2\x88\x95 in directory"),
      "subdir_feed.json");

  EXPECT_FALSE(FindFile(FilePath(FILE_PATH_LITERAL(
      "gdata/Slash / in file 1.txt"))));

  EXPECT_TRUE(FindFile(FilePath::FromUTF8Unsafe(
      "gdata/Slash \xE2\x88\x95 in file 1.txt")));

  EXPECT_TRUE(FindFile(FilePath::FromUTF8Unsafe(
      "gdata/Slash \xE2\x88\x95 in directory/SubDirectory File 1.txt")));
}

TEST_F(GDataFileSystemTest, SearchExistingDocument) {
  LoadRootFeedDocument("root_feed.json");
  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate =
      new MockFindFileDelegate();

  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath(FILE_PATH_LITERAL("gdata")), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));
  EXPECT_CALL(*mock_find_file_delegate.get(), OnFileFound(_))
      .Times(1);

  file_system_->FindFileByPath(
      FilePath(FILE_PATH_LITERAL("gdata/Document 1.gdoc")),
      mock_find_file_delegate);
}

TEST_F(GDataFileSystemTest, SearchDuplicateNames) {
  LoadRootFeedDocument("root_feed.json");

  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate =
      new MockFindFileDelegate();
  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath(FILE_PATH_LITERAL("gdata")), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));
  EXPECT_CALL(*mock_find_file_delegate.get(), OnFileFound(_))
      .Times(1);
  file_system_->FindFileByPath(
      FilePath(FILE_PATH_LITERAL("gdata/Duplicate Name.txt")),
      mock_find_file_delegate);

  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate2 =
      new MockFindFileDelegate();
  EXPECT_CALL(*mock_find_file_delegate2.get(),
              OnEnterDirectory(FilePath(FILE_PATH_LITERAL("gdata")), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));
  EXPECT_CALL(*mock_find_file_delegate2.get(), OnFileFound(_))
      .Times(1);
  file_system_->FindFileByPath(
      FilePath(FILE_PATH_LITERAL("gdata/Duplicate Name (2).txt")),
      mock_find_file_delegate2);
}

TEST_F(GDataFileSystemTest, SearchExistingDirectory) {
  LoadRootFeedDocument("root_feed.json");
  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate =
      new MockFindFileDelegate();

  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath(FILE_PATH_LITERAL("gdata")), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));
  EXPECT_CALL(*mock_find_file_delegate.get(), OnDirectoryFound(_, _))
      .Times(1);

  file_system_->FindFileByPath(FilePath(FILE_PATH_LITERAL("gdata/Directory 1")),
                               mock_find_file_delegate);
}


TEST_F(GDataFileSystemTest, SearchNonExistingFile) {
  LoadRootFeedDocument("root_feed.json");
  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate =
      new MockFindFileDelegate();

  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath(FILE_PATH_LITERAL("gdata")), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));
  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnError(base::PLATFORM_FILE_ERROR_NOT_FOUND))
      .Times(1);

  file_system_->FindFileByPath(
      FilePath(FILE_PATH_LITERAL("gdata/nonexisting.file")),
      mock_find_file_delegate);
}

TEST_F(GDataFileSystemTest, StopFileSearch) {
  LoadRootFeedDocument("root_feed.json");
  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate =
      new MockFindFileDelegate();

  // Stop on first directory entry.
  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath(FILE_PATH_LITERAL("gdata")), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_TERMINATES));

  file_system_->FindFileByPath(FilePath(FILE_PATH_LITERAL("gdata/Directory 1")),
                               mock_find_file_delegate);
}

TEST_F(GDataFileSystemTest, SearchInSubdir) {
  LoadRootFeedDocument("root_feed.json");
  LoadSubdirFeedDocument(FilePath(FILE_PATH_LITERAL("gdata/Directory 1")),
                         "subdir_feed.json");

  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate =
      new MockFindFileDelegate();

  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath(FILE_PATH_LITERAL("gdata")), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));

  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath(FILE_PATH_LITERAL("gdata/Directory 1")),
                               _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));

  EXPECT_CALL(*mock_find_file_delegate.get(), OnFileFound(_))
      .Times(1);

  file_system_->FindFileByPath(
      FilePath(FILE_PATH_LITERAL("gdata/Directory 1/SubDirectory File 1.txt")),
      mock_find_file_delegate);
}

TEST_F(GDataFileSystemTest, FilePathTests) {
  LoadRootFeedDocument("root_feed.json");
  LoadSubdirFeedDocument(FilePath(FILE_PATH_LITERAL("gdata/Directory 1")),
                         "subdir_feed.json");

  FindAndTestFilePath(FilePath(FILE_PATH_LITERAL("gdata/File 1.txt")));
  FindAndTestFilePath(FilePath(FILE_PATH_LITERAL("gdata/Directory 1")));
  FindAndTestFilePath(
      FilePath(FILE_PATH_LITERAL("gdata/Directory 1/SubDirectory File 1.txt")));
}

TEST_F(GDataFileSystemTest, RemoveFiles) {
  LoadRootFeedDocument("root_feed.json");
  LoadSubdirFeedDocument(FilePath(FILE_PATH_LITERAL("gdata/Directory 1")),
                         "subdir_feed.json");

  FilePath nonexisting_file(FILE_PATH_LITERAL("gdata/Dummy file.txt"));
  FilePath file_in_root(FILE_PATH_LITERAL("gdata/File 1.txt"));
  FilePath dir_in_root(FILE_PATH_LITERAL("gdata/Directory 1"));
  FilePath file_in_subdir(
      FILE_PATH_LITERAL("gdata/Directory 1/SubDirectory File 1.txt"));

  GDataFileBase* file = NULL;
  EXPECT_TRUE((file = FindFile(file_in_root)) != NULL);
  EXPECT_TRUE(file->AsGDataFile() != NULL);
  std::string file_in_root_resource = file->AsGDataFile()->resource_id();
  EXPECT_EQ(file, FindFileByResource(file_in_root_resource));

  EXPECT_TRUE(FindFile(dir_in_root) != NULL);

  EXPECT_TRUE((file = FindFile(file_in_subdir)) != NULL);
  EXPECT_TRUE(file->AsGDataFile() != NULL);
  std::string file_in_subdir_resource = file->AsGDataFile()->resource_id();
  EXPECT_EQ(file, FindFileByResource(file_in_subdir_resource));

  // Remove first file in root.
  EXPECT_TRUE(RemoveFile(file_in_root));
  EXPECT_TRUE(FindFile(file_in_root) == NULL);
  EXPECT_EQ(NULL, FindFileByResource(file_in_root_resource));
  EXPECT_TRUE(FindFile(dir_in_root) != NULL);
  EXPECT_TRUE((file = FindFile(file_in_subdir)) != NULL);
  EXPECT_EQ(file, FindFileByResource(file_in_subdir_resource));

  // Remove directory.
  EXPECT_TRUE(RemoveFile(dir_in_root));
  EXPECT_TRUE(FindFile(file_in_root) == NULL);
  EXPECT_EQ(NULL, FindFileByResource(file_in_root_resource));
  EXPECT_TRUE(FindFile(dir_in_root) == NULL);
  EXPECT_TRUE(FindFile(file_in_subdir) == NULL);
  EXPECT_EQ(NULL, FindFileByResource(file_in_subdir_resource));

  // Try removing file in already removed subdirectory.
  EXPECT_FALSE(RemoveFile(file_in_subdir));

  // Try removing non-existing file.
  EXPECT_FALSE(RemoveFile(nonexisting_file));

  // Try removing root file element.
  EXPECT_FALSE(RemoveFile(FilePath(FILE_PATH_LITERAL("gdata"))));
}

TEST_F(GDataFileSystemTest, CreateDirectory) {
  LoadRootFeedDocument("root_feed.json");
  LoadSubdirFeedDocument(FilePath(FILE_PATH_LITERAL("gdata/Directory 1")),
                         "subdir_feed.json");

  // Create directory in root.
  FilePath dir_path(FILE_PATH_LITERAL("gdata/New Folder 1"));
  EXPECT_TRUE(FindFile(dir_path) == NULL);
  AddDirectoryFromFile(dir_path, "directory_entry_atom.json");
  EXPECT_TRUE(FindFile(dir_path) != NULL);

  // Create directory in a sub dirrectory.
  FilePath subdir_path(FILE_PATH_LITERAL("gdata/New Folder 1/New Folder 2"));
  EXPECT_TRUE(FindFile(subdir_path) == NULL);
  AddDirectoryFromFile(subdir_path, "directory_entry_atom.json");
  EXPECT_TRUE(FindFile(subdir_path) != NULL);
}

TEST_F(GDataFileSystemTest, FindFirstMissingParentDirectory) {
  LoadRootFeedDocument("root_feed.json");
  LoadSubdirFeedDocument(FilePath(FILE_PATH_LITERAL("gdata/Directory 1")),
                         "subdir_feed.json");

  GURL last_dir_content_url;
  FilePath first_missing_parent_path;

  // Create directory in root.
  FilePath dir_path(FILE_PATH_LITERAL("gdata/New Folder 1"));
  EXPECT_EQ(
      GDataFileSystem::FOUND_MISSING,
      file_system_->FindFirstMissingParentDirectory(dir_path,
          &last_dir_content_url,
          &first_missing_parent_path));
  EXPECT_EQ(FilePath(FILE_PATH_LITERAL("gdata/New Folder 1")),
            first_missing_parent_path);
  EXPECT_TRUE(last_dir_content_url.is_empty());    // root directory.

  // Missing folders in subdir of an existing folder.
  FilePath dir_path2(FILE_PATH_LITERAL("gdata/Directory 1/New Folder 2"));
  EXPECT_EQ(
      GDataFileSystem::FOUND_MISSING,
      file_system_->FindFirstMissingParentDirectory(dir_path2,
          &last_dir_content_url,
          &first_missing_parent_path));
  EXPECT_EQ(FilePath(FILE_PATH_LITERAL("gdata/Directory 1/New Folder 2")),
            first_missing_parent_path);
  EXPECT_FALSE(last_dir_content_url.is_empty());    // non-root directory.

  // Missing two folders on the path.
  FilePath dir_path3 = dir_path2.Append(FILE_PATH_LITERAL("Another Folder"));
  EXPECT_EQ(
      GDataFileSystem::FOUND_MISSING,
      file_system_->FindFirstMissingParentDirectory(dir_path3,
          &last_dir_content_url,
          &first_missing_parent_path));
  EXPECT_EQ(FilePath(FILE_PATH_LITERAL("gdata/Directory 1/New Folder 2")),
            first_missing_parent_path);
  EXPECT_FALSE(last_dir_content_url.is_empty());    // non-root directory.

  // Folders on top of an existing file.
  EXPECT_EQ(
      GDataFileSystem::FOUND_INVALID,
      file_system_->FindFirstMissingParentDirectory(
          FilePath(FILE_PATH_LITERAL("gdata/File 1.txt/BadDir")),
          &last_dir_content_url,
          &first_missing_parent_path));

  // Existing folder.
  EXPECT_EQ(
      GDataFileSystem::DIRECTORY_ALREADY_PRESENT,
      file_system_->FindFirstMissingParentDirectory(
          FilePath(FILE_PATH_LITERAL("gdata/Directory 1")),
          &last_dir_content_url,
          &first_missing_parent_path));
}

TEST_F(GDataFileSystemTest, GetCacheFilePathAlphanumeric) {
  std::string res_id("pdf:1a2b");
  std::string md5("abcd12345");
  TestGetCacheFilePath(res_id, md5,
                       res_id + FilePath::kExtensionSeparator + md5);
}

TEST_F(GDataFileSystemTest, GetCacheFilePathNonAlphanumeric) {
  std::string res_id("pdf:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?");
  std::string md5("?/>,<';\\}]{[+=_-)(*&^%$#@!~`");
  std::string escaped_res_id;
  ReplaceChars(res_id, kSlash, std::string(kEscapedSlash), &escaped_res_id);
  std::string escaped_md5;
  ReplaceChars(md5, kSlash, std::string(kEscapedSlash), &escaped_md5);
  TestGetCacheFilePath(res_id, md5,
                       escaped_res_id + FilePath::kExtensionSeparator +
                       escaped_md5);
}

TEST_F(GDataFileSystemTest, StoreToCache) {
  std::string res_id("pdf:1a2b");
  std::string md5("abcd12345");
  TestStoreToCache(res_id, md5, GetTestFilePath("root_feed.json"));
}

TEST_F(GDataFileSystemTest, GetFromCache) {
  std::string res_id("pdf:1a2b");
  std::string md5("abcd12345");
  TestStoreToCache(res_id, md5, GetTestFilePath("root_feed.json"));
  TestGetFromCache(res_id, md5);
}

TEST_F(GDataFileSystemTest, RemoveFromCache) {
  std::string res_id("pdf:1a2b");
  std::string md5("abcd12345");
  TestStoreToCache(res_id, md5, GetTestFilePath("root_feed.json"));
  TestRemoveFromCache(res_id);
}

TEST_F(GDataFileSystemTest, Pin) {
  std::string res_id("pdf:1a2b");
  std::string md5("abcd12345");
  TestStoreToCache(res_id, md5, GetTestFilePath("root_feed.json"));
  TestPin(res_id, md5);
}

TEST_F(GDataFileSystemTest, Unpin) {
  std::string res_id("pdf:1a2b");
  std::string md5("abcd12345");
  TestStoreToCache(res_id, md5, GetTestFilePath("root_feed.json"));
  TestPin(res_id, md5);
  TestUnpin(res_id, md5);
}

// TODO(kuan): Write invalid cache unittests.

// TODO(satorux): Write a test for GetFile() once DocumentsService is
// mockable.

TEST_F(GDataFileSystemTest, GetGDataFileInfoFromPath) {
  LoadRootFeedDocument("root_feed.json");

  // Lock to call GetGDataFileInfoFromPath.
  base::AutoLock lock(file_system_->lock_);
  GDataFileBase* file_info = file_system_->GetGDataFileInfoFromPath(
      FilePath(FILE_PATH_LITERAL("gdata/File 1.txt")));
  ASSERT_TRUE(file_info != NULL);
  EXPECT_EQ("https://file_link_self/", file_info->self_url().spec());
  EXPECT_EQ("https://file_content_url/", file_info->content_url().spec());

  GDataFileBase* non_existent = file_system_->GetGDataFileInfoFromPath(
      FilePath(FILE_PATH_LITERAL("gdata/Nonexistent.txt")));
  ASSERT_TRUE(non_existent == NULL);
}

TEST_F(GDataFileSystemTest, GetFromCacheForPath) {
  LoadRootFeedDocument("root_feed.json");

  // First make sure the file exists in GData.
  FilePath gdata_file_path = FilePath(FILE_PATH_LITERAL("gdata/File 1.txt"));
  GDataFile* file = NULL;
  {  // Lock to call GetGDataFileInfoFromPath.
    base::AutoLock lock(file_system_->lock_);
    GDataFileBase* file_info =
        file_system_->GetGDataFileInfoFromPath(gdata_file_path);
    ASSERT_TRUE(file_info != NULL);
    file = file_info->AsGDataFile();
    ASSERT_TRUE(file != NULL);
  }

  // A file that exists in GData but not in cache.
  FilePath cache_file_path = file_system_->GetFromCacheForPath(gdata_file_path);
  EXPECT_TRUE(cache_file_path.empty());

  // Store a file corresponding to resource and md5 of "gdata/File 1.txt" to
  // cache.
  TestStoreToCache(file->resource_id(), file->file_md5(),
                   GetTestFilePath("root_feed.json"));

  // Now the file should exist in cache.
  cache_file_path = file_system_->GetFromCacheForPath(gdata_file_path);
  EXPECT_EQ(file_system_->GetCacheFilePath(file->resource_id(),
                                           file->file_md5()),
            cache_file_path);

  // A file that doesn't exist in gdata.
  cache_file_path = file_system_->GetFromCacheForPath(
      FilePath(FILE_PATH_LITERAL("gdata/Nonexistent.txt")));
  EXPECT_TRUE(cache_file_path.empty());
}

// Create a directory through the document service
TEST_F(GDataFileSystemTest, CreateDirectoryWithService) {
  LoadRootFeedDocument("root_feed.json");
  EXPECT_CALL(*mock_doc_service_,
              CreateDirectory(_, "Sample Directory Title", _)).Times(1);

  // Set last error so it's not a valid error code.
  callback_helper_->last_error_ = static_cast<base::PlatformFileError>(1);
  file_system_->CreateDirectory(
      FilePath(FILE_PATH_LITERAL("gdata/Sample Directory Title")),
      false,  // is_exclusive
      true,  // is_recursive
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get()));
  message_loop_.RunAllPending();  // Wait to get our result.
  // TODO(gspencer): Uncomment this when we get a blob that
  // works that can be returned from the mock.
  // EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);
}

TEST_F(GDataFileSystemTest, GetFile) {
  LoadRootFeedDocument("root_feed.json");

  GDataFileSystem::GetFileCallback callback =
      base::Bind(&CallbackHelper::GetFileCallback,
                 callback_helper_.get());

  EXPECT_CALL(*mock_doc_service_,
              DownloadFile(GURL("https://file_content_url/"), _));

  FilePath file_in_root(FILE_PATH_LITERAL("gdata/File 1.txt"));
  file_system_->GetFile(file_in_root, callback);
  message_loop_.RunAllPending();  // Wait to get our result.
  EXPECT_STREQ("file_content_url/",
               callback_helper_->download_path_.value().c_str());
}

class GDataFileSystemInitCacheTest : public GDataFileSystemTestBase {
 protected:
  GDataFileSystemInitCacheTest() {
  }

  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile);
    // TODO(kuan): Create some cache files to trigger scanning of cache dir,
    // and write tests to check the cache map.
    Init(profile_.get());
  }
};

}   // namespace gdata
