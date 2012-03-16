// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <errno.h>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/bind_helpers.h"
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

namespace {

const char kSlash[] = "/";
const char kEscapedSlash[] = "\xE2\x88\x95";

struct InitCacheRec {
  const char* source;
  const char* resource;
  const char* md5;
  mode_t mode_bits_to_enable;
  int cache_status;
} const init_cache_table[] = {
  { "root_feed.json", "file_1_resource", "file_1_md5",
    S_IROTH, gdata::GDataFile::CACHE_STATE_PRESENT },
  { "subdir_feed.json", "file_2_resource", "file_2_md5",
    S_IROTH | S_IWOTH,
    gdata::GDataFile::CACHE_STATE_PRESENT |
    gdata::GDataFile::CACHE_STATE_DIRTY },
  { "directory_entry_atom.json", "pdf:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?",
    "abcdef0123456789", S_IROTH | S_IXOTH,
    gdata::GDataFile::CACHE_STATE_PRESENT |
    gdata::GDataFile::CACHE_STATE_PINNED },
};

}  // anonymous namespace

namespace gdata {

class GDataFileSystemTest : public testing::Test {
 protected:
  GDataFileSystemTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_),
        file_system_(NULL),
        num_callback_invocations_(0),
        expected_error_(base::PLATFORM_FILE_OK),
        expected_cache_state_(0),
        expected_file_(NULL) {
  }

  virtual void SetUp() OVERRIDE {
    profile_.reset(new TestingProfile);

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
        list, FROM_SERVER) == base::PLATFORM_FILE_OK;
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
    ASSERT_TRUE(file) << "File can't be found " << file_path.value();
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
                        const FilePath& source_path,
                        base::PlatformFileError expected_error) {
    expected_error_ = expected_error;

    FilePath temp_path = profile_->GetPath();
    temp_path = temp_path.Append(source_path.BaseName());

    if (expected_error_ != base::PLATFORM_FILE_ERROR_NOT_FOUND) {
      // StoreToCache will move |source_path| to the cache dir; in order to keep
      // the test data file where it is, we need to copy |source_path| to
      // the profile dir and use that file for StoreToCache instead.
      EXPECT_TRUE(file_util::CopyFile(source_path, temp_path));
    }

    file_system_->StoreToCache(res_id, md5, temp_path,
        base::Bind(&GDataFileSystemTest::VerifyStoreToCache,
                   base::Unretained(this)));

    RunAllPendingForCache();
  }

  void VerifyStoreToCache(base::PlatformFileError error,
                          const std::string& res_id,
                          const std::string& md5) {
    ++num_callback_invocations_;

    EXPECT_EQ(expected_error_, error);
    VerifyCacheFileStatusAndMode(res_id, md5, GDataFile::CACHE_STATE_PRESENT,
                                 S_IROTH);
  }

  void TestGetFromCache(const std::string& res_id, const std::string& md5,
                        base::PlatformFileError expected_error) {
    expected_error_ = expected_error;

    file_system_->GetFromCache(res_id, md5,
        base::Bind(&GDataFileSystemTest::VerifyGetFromCache,
                   base::Unretained(this)));

    RunAllPendingForCache();
  }

  void TestGetFromCacheForPath(const FilePath& gdata_file_path,
                               base::PlatformFileError expected_error) {
    expected_error_ = expected_error;

    file_system_->GetFromCacheForPath(gdata_file_path,
        base::Bind(&GDataFileSystemTest::VerifyGetFromCache,
                   base::Unretained(this)));

    RunAllPendingForCache();
  }

  void VerifyGetFromCache(base::PlatformFileError error,
                          const std::string& res_id,
                          const std::string& md5,
                          const FilePath& gdata_file_path,
                          const FilePath& cache_file_path) {
    ++num_callback_invocations_;

    EXPECT_EQ(expected_error_, error);

    if (error == base::PLATFORM_FILE_OK) {
      // Verify filename of |cache_file_path|.
      FilePath base_name = cache_file_path.BaseName();
      EXPECT_EQ(GDataFileBase::EscapeUtf8FileName(res_id) +
                FilePath::kExtensionSeparator +
                GDataFileBase::EscapeUtf8FileName(md5),
                base_name.value());
    } else {
      EXPECT_TRUE(cache_file_path.empty());
    }
  }

  void TestRemoveFromCache(const std::string& res_id,
                           base::PlatformFileError expected_error) {
    expected_error_ = expected_error;

    file_system_->RemoveFromCache(res_id,
        base::Bind(&GDataFileSystemTest::VerifyRemoveFromCache,
                   base::Unretained(this)));

    RunAllPendingForCache();
  }

  void VerifyRemoveFromCache(base::PlatformFileError error,
                             const std::string& res_id,
                             const std::string& md5) {
    ++num_callback_invocations_;

    EXPECT_EQ(expected_error_, error);

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

  void TestPin(const std::string& res_id, const std::string& md5,
               base::PlatformFileError expected_error) {
    expected_error_ = expected_error;

    file_system_->Pin(res_id, md5,
        base::Bind(&GDataFileSystemTest::VerifyPin,
                   base::Unretained(this)));

    RunAllPendingForCache();
  }

  void VerifyPin(base::PlatformFileError error, const std::string& res_id,
                 const std::string& md5) {
    ++num_callback_invocations_;

    EXPECT_EQ(expected_error_, error);
    if (error == base::PLATFORM_FILE_OK) {
      VerifyCacheFileStatusAndMode(res_id, md5,
                                   GDataFile::CACHE_STATE_PRESENT |
                                   GDataFile::CACHE_STATE_PINNED,
                                   S_IROTH | S_IXOTH);
    }
  }

  void TestUnpin(const std::string& res_id, const std::string& md5,
               base::PlatformFileError expected_error) {
    expected_error_ = expected_error;

    file_system_->Unpin(res_id, md5,
        base::Bind(&GDataFileSystemTest::VerifyUnpin,
                   base::Unretained(this)));

    RunAllPendingForCache();
  }

  void VerifyUnpin(base::PlatformFileError error, const std::string& res_id,
                  const std::string& md5) {
    ++num_callback_invocations_;

    EXPECT_EQ(expected_error_, error);
    if (error == base::PLATFORM_FILE_OK) {
      VerifyCacheFileStatusAndMode(res_id, md5, GDataFile::CACHE_STATE_PRESENT,
                                   S_IROTH);
    }
  }

  void TestGetCacheState(const std::string& res_id, const std::string& md5,
                         base::PlatformFileError expected_error,
                         int expected_cache_state, GDataFile* expected_file) {
    expected_error_ = expected_error;
    expected_cache_state_ = expected_cache_state;
    expected_file_ = expected_file;

    file_system_->GetCacheState(res_id, md5,
        base::Bind(&GDataFileSystemTest::VerifyGetCacheState,
                   base::Unretained(this)));

    RunAllPendingForCache();
  }

  void VerifyGetCacheState(base::PlatformFileError error, GDataFile* file,
                           int cache_state) {
    ++num_callback_invocations_;

    EXPECT_EQ(expected_error_, error);

    if (error == base::PLATFORM_FILE_OK) {
      EXPECT_EQ(expected_cache_state_, cache_state);
      EXPECT_EQ(expected_file_, file);
    }
  }

  void PrepareForInitCacheTest() {
    // Create gdata cache blobs directory.
    ASSERT_TRUE(file_util::CreateDirectory(
        file_system_->cache_paths_[GDataFileSystem::CACHE_TYPE_BLOBS]));

    // Dump some files into cache blob dir so that
    // GDataFileSystem::InitializeCacheIOThreadPool would scan through them and
    // populate cache map accordingly.

    // Determine gdata cache blobs absolute path.
    // Copy files from data dir to cache dir to act as cached files.
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(init_cache_table); ++i) {
      // Copy file from data dir to cache dir, naming it per cache files
      // convention.
      FilePath source_path = GetTestFilePath(init_cache_table[i].source);
      FilePath dest_path = file_system_->GetCacheFilePath(
          init_cache_table[i].resource, init_cache_table[i].md5);
      ASSERT_TRUE(file_util::CopyFile(source_path, dest_path));

      // Change mode of cached file.
      struct stat64 stat_buf;
      ASSERT_FALSE(stat64(dest_path.value().c_str(), &stat_buf));
      ASSERT_FALSE(chmod(dest_path.value().c_str(),
                         stat_buf.st_mode |
                             init_cache_table[i].mode_bits_to_enable));
    }
  }

  void TestInitializeCache() {
    for (size_t i = 0; i < ARRAYSIZE_UNSAFE(init_cache_table); ++i) {
      // Check that cached file exists.
      num_callback_invocations_ = 0;
      TestGetFromCache(init_cache_table[i].resource, init_cache_table[i].md5,
                       base::PLATFORM_FILE_OK);
      EXPECT_EQ(1, num_callback_invocations_);

      // Verify status of cached file.
      EXPECT_EQ(init_cache_table[i].cache_status,
                file_system_->root_->GetCacheState(init_cache_table[i].resource,
                                                   init_cache_table[i].md5));
    }
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

  void TestLoadMetadataFromCache(const FilePath::StringType& feeds_path,
                                 const FilePath& meta_cache_path) {
    FilePath file_path = GetTestFilePath(feeds_path);
    // Move test file into the correct cache location first.
    FilePath cache_dir_path = profile_->GetPath();
    cache_dir_path = cache_dir_path.Append(meta_cache_path).DirName();
    ASSERT_TRUE(file_util::CreateDirectory(cache_dir_path));
    ASSERT_TRUE(file_util::CopyFile(file_path,
        cache_dir_path.Append(meta_cache_path.BaseName())));

    scoped_refptr<ReadOnlyFindFileDelegate> delegate(
        new ReadOnlyFindFileDelegate());
    GDataFileSystem::FindFileParams params(
        FilePath(FILE_PATH_LITERAL("gdata")),
        true,
        FilePath(FILE_PATH_LITERAL("gdata")),
        GURL(),
        true,
        delegate);

    file_system_->LoadRootFeed(params);
    RunAllPendingForCache();
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

  static FilePath GetTestFilePath(const FilePath::StringType& filename) {
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
    CallbackHelper()
        : last_error_(base::PLATFORM_FILE_OK),
          quota_bytes_total_(0),
          quota_bytes_used_(0) {}
    virtual ~CallbackHelper() {}
    virtual void GetFileCallback(base::PlatformFileError error,
                                 const FilePath& file_path) {
      last_error_ = error;
      download_path_ = file_path;
    }
    virtual void FileOperationCallback(base::PlatformFileError error) {
      DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

      last_error_ = error;
    }
    virtual void GetAvailableSpaceCallback(base::PlatformFileError error,
                                           int bytes_total,
                                           int bytes_used) {
      last_error_ = error;
      quota_bytes_total_ = bytes_total;
      quota_bytes_used_ = bytes_used;
    }

    base::PlatformFileError last_error_;
    FilePath download_path_;
    int quota_bytes_total_;
    int quota_bytes_used_;
  };

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_ptr<TestingProfile> profile_;
  scoped_refptr<CallbackHelper> callback_helper_;
  GDataFileSystem* file_system_;
  MockDocumentsService* mock_doc_service_;

  int num_callback_invocations_;
  base::PlatformFileError expected_error_;
  int expected_cache_state_;
  GDataFile* expected_file_;
};

// Delegate used to find a directory element for file system updates.
class MockFindFileDelegate : public gdata::FindFileDelegate {
 public:
  MockFindFileDelegate() {
  }

  virtual ~MockFindFileDelegate() {
  }

  // gdata::FindFileDelegate overrides.
  MOCK_METHOD1(OnFileFound, void(GDataFile* file));
  MOCK_METHOD2(OnDirectoryFound, void(const FilePath&, GDataDirectory* dir));
  MOCK_METHOD2(OnEnterDirectory, FindFileTraversalCommand(
      const FilePath&, GDataDirectory* dir));
  MOCK_METHOD1(OnError, void(base::PlatformFileError));
  MOCK_CONST_METHOD0(had_terminated, bool());
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

TEST_F(GDataFileSystemTest, SearchEncodedFileNamesLoadingRoot) {
  LoadRootFeedDocument("root_feed.json");

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

// Check the reconstruction of the directory structure from only the root feed.
TEST_F(GDataFileSystemTest, SearchInSubSubdir) {
  LoadRootFeedDocument("root_feed.json");

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

  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath(FILE_PATH_LITERAL(
                                   "gdata/Directory 1/Sub Directory Folder")),
                               _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));

  EXPECT_CALL(*mock_find_file_delegate.get(), OnDirectoryFound(_, _))
      .Times(1);

  file_system_->FindFileByPath(
      FilePath(FILE_PATH_LITERAL("gdata/Directory 1/Sub Directory Folder/"
                                 "Sub Sub Directory Folder")),
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

TEST_F(GDataFileSystemTest, CachedFeedLoading) {
  TestLoadMetadataFromCache(FILE_PATH_LITERAL("cached_feeds.json"),
      FilePath(FILE_PATH_LITERAL("GCache/v1/meta/last_feed.json")));

  // Test first feed elements.
  FindAndTestFilePath(FilePath(FILE_PATH_LITERAL("gdata/Feed 1 File.txt")));
  FindAndTestFilePath(
      FilePath(FILE_PATH_LITERAL(
          "gdata/Directory 1/Feed 1 SubDirectory File.txt")));

  // Test second feed elements.
  FindAndTestFilePath(FilePath(FILE_PATH_LITERAL("gdata/Feed 2 File.txt")));
  FindAndTestFilePath(
      FilePath(FILE_PATH_LITERAL(
          "gdata/Directory 1/Sub Directory Folder/Feed 2 Directory")));
}

TEST_F(GDataFileSystemTest, CopyNotExistingFile) {
  FilePath src_file_path(FILE_PATH_LITERAL("gdata/Dummy file.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL("gdata/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  GDataFileBase* src_file = NULL;
  EXPECT_TRUE((src_file = FindFile(src_file_path)) == NULL);

  GDataFileSystem::FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Copy(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);

  EXPECT_TRUE(FindFile(src_file_path) == NULL);
  EXPECT_TRUE(FindFile(dest_file_path) == NULL);
}

TEST_F(GDataFileSystemTest, CopyFileToNonExistingDirectory) {
  FilePath src_file_path(FILE_PATH_LITERAL("gdata/File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("gdata/Dummy"));
  FilePath dest_file_path(FILE_PATH_LITERAL("gdata/Dummy/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  GDataFileBase* src_file = NULL;
  EXPECT_TRUE((src_file = FindFile(src_file_path)) != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_path_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindFileByResource(src_file_path_resource));
  EXPECT_FALSE(src_file->self_url().is_empty());

  EXPECT_TRUE(FindFile(dest_parent_path) == NULL);

  GDataFileSystem::FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);

  EXPECT_EQ(src_file, FindFile(src_file_path));
  EXPECT_EQ(src_file, FindFileByResource(src_file_path_resource));

  EXPECT_TRUE(FindFile(dest_parent_path) == NULL);
  EXPECT_TRUE(FindFile(dest_file_path) == NULL);
}

// Test the case where the parent of |dest_file_path| is a existing file,
// not a directory.
TEST_F(GDataFileSystemTest, CopyFileToInvalidPath) {
  FilePath src_file_path(FILE_PATH_LITERAL("gdata/Document 1.gdoc"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("gdata/Duplicate Name.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL(
      "gdata/Duplicate Name.txt/Document 1.gdoc"));

  LoadRootFeedDocument("root_feed.json");

  GDataFileBase* src_file = NULL;
  EXPECT_TRUE((src_file = FindFile(src_file_path)) != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_path_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindFileByResource(src_file_path_resource));
  EXPECT_FALSE(src_file->self_url().is_empty());

  GDataFileBase* dest_parent = NULL;
  EXPECT_TRUE((dest_parent = FindFile(dest_parent_path)) != NULL);
  EXPECT_TRUE(dest_parent->AsGDataFile() != NULL);

  GDataFileSystem::FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Copy(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY,
            callback_helper_->last_error_);

  EXPECT_EQ(src_file, FindFile(src_file_path));
  EXPECT_EQ(src_file, FindFileByResource(src_file_path_resource));
  EXPECT_EQ(dest_parent, FindFile(dest_parent_path));

  EXPECT_TRUE(FindFile(dest_file_path) == NULL);
}

TEST_F(GDataFileSystemTest, RenameFile) {
  FilePath src_file_path(
      FILE_PATH_LITERAL("gdata/Directory 1/SubDirectory File 1.txt"));
  FilePath src_parent_path(FILE_PATH_LITERAL("gdata/Directory 1"));
  FilePath dest_file_path(FILE_PATH_LITERAL("gdata/Directory 1/Test.log"));

  LoadRootFeedDocument("root_feed.json");
  LoadSubdirFeedDocument(src_parent_path, "subdir_feed.json");

  GDataFileBase* src_file = NULL;
  EXPECT_TRUE((src_file = FindFile(src_file_path)) != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindFileByResource(src_file_resource));

  EXPECT_CALL(*mock_doc_service_,
              RenameResource(src_file->self_url(),
                             FILE_PATH_LITERAL("Test.log"), _));

  GDataFileSystem::FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result.
  EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);

  EXPECT_TRUE(FindFile(src_file_path) == NULL);

  GDataFileBase* dest_file = NULL;
  EXPECT_TRUE((dest_file = FindFile(dest_file_path)) != NULL);
  EXPECT_EQ(dest_file, FindFileByResource(src_file_resource));
  EXPECT_EQ(src_file, dest_file);
}

TEST_F(GDataFileSystemTest, MoveFileFromRootToSubDirectory) {
  FilePath src_file_path(FILE_PATH_LITERAL("gdata/File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("gdata/Directory 1"));
  FilePath dest_file_path(FILE_PATH_LITERAL("gdata/Directory 1/Test.log"));

  LoadRootFeedDocument("root_feed.json");
  LoadSubdirFeedDocument(dest_parent_path, "subdir_feed.json");

  GDataFileBase* src_file = NULL;
  EXPECT_TRUE((src_file = FindFile(src_file_path)) != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_path_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindFileByResource(src_file_path_resource));
  EXPECT_FALSE(src_file->self_url().is_empty());

  GDataFileBase* dest_parent = NULL;
  EXPECT_TRUE((dest_parent = FindFile(dest_parent_path)) != NULL);
  EXPECT_TRUE(dest_parent->AsGDataDirectory() != NULL);
  EXPECT_FALSE(dest_parent->content_url().is_empty());

  EXPECT_CALL(*mock_doc_service_,
              RenameResource(src_file->self_url(),
                             FILE_PATH_LITERAL("Test.log"), _));
  EXPECT_CALL(*mock_doc_service_,
              AddResourceToDirectory(dest_parent->content_url(),
                                     src_file->self_url(), _));

  GDataFileSystem::FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result.
  EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);

  EXPECT_TRUE(FindFile(src_file_path) == NULL);

  GDataFileBase* dest_file = NULL;
  EXPECT_TRUE((dest_file = FindFile(dest_file_path)) != NULL);
  EXPECT_EQ(dest_file, FindFileByResource(src_file_path_resource));
  EXPECT_EQ(src_file, dest_file);
}

TEST_F(GDataFileSystemTest, MoveFileFromSubDirectoryToRoot) {
  FilePath src_parent_path(FILE_PATH_LITERAL("gdata/Directory 1"));
  FilePath src_file_path(
      FILE_PATH_LITERAL("gdata/Directory 1/SubDirectory File 1.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL("gdata/Test.log"));

  LoadRootFeedDocument("root_feed.json");
  LoadSubdirFeedDocument(src_parent_path, "subdir_feed.json");

  GDataFileBase* src_file = NULL;
  EXPECT_TRUE((src_file = FindFile(src_file_path)) != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_path_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindFileByResource(src_file_path_resource));
  EXPECT_FALSE(src_file->self_url().is_empty());

  GDataFileBase* src_parent = NULL;
  EXPECT_TRUE((src_parent = FindFile(src_parent_path)) != NULL);
  EXPECT_TRUE(src_parent->AsGDataDirectory() != NULL);
  EXPECT_FALSE(src_parent->content_url().is_empty());

  EXPECT_CALL(*mock_doc_service_,
              RenameResource(src_file->self_url(),
                             FILE_PATH_LITERAL("Test.log"), _));
  EXPECT_CALL(*mock_doc_service_,
              RemoveResourceFromDirectory(src_parent->content_url(),
                                          src_file->self_url(),
                                          src_file_path_resource, _));

  GDataFileSystem::FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result.
  EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);

  EXPECT_TRUE(FindFile(src_file_path) == NULL);

  GDataFileBase* dest_file = NULL;
  EXPECT_TRUE((dest_file = FindFile(dest_file_path)) != NULL);
  EXPECT_EQ(dest_file, FindFileByResource(src_file_path_resource));
  EXPECT_EQ(src_file, dest_file);
}

TEST_F(GDataFileSystemTest, MoveFileBetweenSubDirectories) {
  FilePath src_parent_path(FILE_PATH_LITERAL("gdata/Directory 1"));
  FilePath src_file_path(
      FILE_PATH_LITERAL("gdata/Directory 1/SubDirectory File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("gdata/New Folder 1"));
  FilePath dest_file_path(FILE_PATH_LITERAL("gdata/New Folder 1/Test.log"));
  FilePath interim_file_path(FILE_PATH_LITERAL("gdata/Test.log"));

  LoadRootFeedDocument("root_feed.json");
  LoadSubdirFeedDocument(src_parent_path, "subdir_feed.json");
  AddDirectoryFromFile(dest_parent_path, "directory_entry_atom.json");

  GDataFileBase* src_file = NULL;
  EXPECT_TRUE((src_file = FindFile(src_file_path)) != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_path_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindFileByResource(src_file_path_resource));
  EXPECT_FALSE(src_file->self_url().is_empty());

  GDataFileBase* src_parent = NULL;
  EXPECT_TRUE((src_parent = FindFile(src_parent_path)) != NULL);
  EXPECT_TRUE(src_parent->AsGDataDirectory() != NULL);
  EXPECT_FALSE(src_parent->content_url().is_empty());

  GDataFileBase* dest_parent = NULL;
  EXPECT_TRUE((dest_parent = FindFile(dest_parent_path)) != NULL);
  EXPECT_TRUE(dest_parent->AsGDataDirectory() != NULL);
  EXPECT_FALSE(dest_parent->content_url().is_empty());

  EXPECT_TRUE(FindFile(interim_file_path) == NULL);

  EXPECT_CALL(*mock_doc_service_,
              RenameResource(src_file->self_url(),
                             FILE_PATH_LITERAL("Test.log"), _));
  EXPECT_CALL(*mock_doc_service_,
              RemoveResourceFromDirectory(src_parent->content_url(),
                                          src_file->self_url(),
                                          src_file_path_resource, _));
  EXPECT_CALL(*mock_doc_service_,
              AddResourceToDirectory(dest_parent->content_url(),
                                     src_file->self_url(), _));

  GDataFileSystem::FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result.
  EXPECT_EQ(base::PLATFORM_FILE_OK, callback_helper_->last_error_);

  EXPECT_TRUE(FindFile(src_file_path) == NULL);
  EXPECT_TRUE(FindFile(interim_file_path) == NULL);

  GDataFileBase* dest_file = NULL;
  EXPECT_TRUE((dest_file = FindFile(dest_file_path)) != NULL);
  EXPECT_EQ(dest_file, FindFileByResource(src_file_path_resource));
  EXPECT_EQ(src_file, dest_file);
}

TEST_F(GDataFileSystemTest, MoveNotExistingFile) {
  FilePath src_file_path(FILE_PATH_LITERAL("gdata/Dummy file.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL("gdata/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  GDataFileBase* src_file = NULL;
  EXPECT_TRUE((src_file = FindFile(src_file_path)) == NULL);

  GDataFileSystem::FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);

  EXPECT_TRUE(FindFile(src_file_path) == NULL);
  EXPECT_TRUE(FindFile(dest_file_path) == NULL);
}

TEST_F(GDataFileSystemTest, MoveFileToNonExistingDirectory) {
  FilePath src_file_path(FILE_PATH_LITERAL("gdata/File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("gdata/Dummy"));
  FilePath dest_file_path(FILE_PATH_LITERAL("gdata/Dummy/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  GDataFileBase* src_file = NULL;
  EXPECT_TRUE((src_file = FindFile(src_file_path)) != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_path_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindFileByResource(src_file_path_resource));
  EXPECT_FALSE(src_file->self_url().is_empty());

  EXPECT_TRUE(FindFile(dest_parent_path) == NULL);

  GDataFileSystem::FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_FOUND, callback_helper_->last_error_);

  EXPECT_EQ(src_file, FindFile(src_file_path));
  EXPECT_EQ(src_file, FindFileByResource(src_file_path_resource));

  EXPECT_TRUE(FindFile(dest_parent_path) == NULL);
  EXPECT_TRUE(FindFile(dest_file_path) == NULL);
}

// Test the case where the parent of |dest_file_path| is a existing file,
// not a directory.
TEST_F(GDataFileSystemTest, MoveFileToInvalidPath) {
  FilePath src_file_path(FILE_PATH_LITERAL("gdata/File 1.txt"));
  FilePath dest_parent_path(FILE_PATH_LITERAL("gdata/Duplicate Name.txt"));
  FilePath dest_file_path(FILE_PATH_LITERAL(
      "gdata/Duplicate Name.txt/Test.log"));

  LoadRootFeedDocument("root_feed.json");

  GDataFileBase* src_file = NULL;
  EXPECT_TRUE((src_file = FindFile(src_file_path)) != NULL);
  EXPECT_TRUE(src_file->AsGDataFile() != NULL);
  std::string src_file_path_resource = src_file->AsGDataFile()->resource_id();
  EXPECT_EQ(src_file, FindFileByResource(src_file_path_resource));
  EXPECT_FALSE(src_file->self_url().is_empty());

  GDataFileBase* dest_parent = NULL;
  EXPECT_TRUE((dest_parent = FindFile(dest_parent_path)) != NULL);
  EXPECT_TRUE(dest_parent->AsGDataFile() != NULL);

  GDataFileSystem::FileOperationCallback callback =
      base::Bind(&CallbackHelper::FileOperationCallback,
                 callback_helper_.get());

  file_system_->Move(src_file_path, dest_file_path, callback);
  message_loop_.RunAllPending();  // Wait to get our result.
  EXPECT_EQ(base::PLATFORM_FILE_ERROR_NOT_A_DIRECTORY,
            callback_helper_->last_error_);

  EXPECT_EQ(src_file, FindFile(src_file_path));
  EXPECT_EQ(src_file, FindFileByResource(src_file_path_resource));
  EXPECT_EQ(dest_parent, FindFile(dest_parent_path));

  EXPECT_TRUE(FindFile(dest_file_path) == NULL);
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

TEST_F(GDataFileSystemTest, GetCacheFilePath) {
  // Use alphanumeric characters for resource id.
  std::string res_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  TestGetCacheFilePath(res_id, md5,
                       res_id + FilePath::kExtensionSeparator + md5);
  EXPECT_EQ(0, num_callback_invocations_);

  // Use non-alphanumeric characters for resource id, including '.' which is an
  // extension separator.
  res_id = "pdf:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?";
  std::string escaped_res_id;
  ReplaceChars(res_id, kSlash, std::string(kEscapedSlash), &escaped_res_id);
  std::string escaped_md5;
  ReplaceChars(md5, kSlash, std::string(kEscapedSlash), &escaped_md5);
  num_callback_invocations_ = 0;
  TestGetCacheFilePath(res_id, md5,
                       escaped_res_id + FilePath::kExtensionSeparator +
                       escaped_md5);
  EXPECT_EQ(0, num_callback_invocations_);
}

TEST_F(GDataFileSystemTest, StoreToCache) {
  std::string res_id("pdf:1a2b");
  std::string md5("abcdef0123456789");

  // Store a file that exists.
  TestStoreToCache(res_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);

  // Store a file that doesn't exist.
  num_callback_invocations_ = 0;
  TestStoreToCache(res_id, md5, FilePath("./non_existent.json"),
                   base::PLATFORM_FILE_ERROR_NOT_FOUND);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataFileSystemTest, GetFromCache) {
  std::string res_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  // First store a file to cache.
  TestStoreToCache(res_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK);

  // Then try to get the existing file from cache.
  num_callback_invocations_ = 0;
  TestGetFromCache(res_id, md5, base::PLATFORM_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);

  // Get file from cache with same resource id as existing file but different
  // md5.
  num_callback_invocations_ = 0;
  TestGetFromCache(res_id, "9999", base::PLATFORM_FILE_ERROR_NOT_FOUND);
  EXPECT_EQ(1, num_callback_invocations_);

  // Get file from cache with different resource id from existing file but same
  // md5.
  num_callback_invocations_ = 0;
  res_id = "document:1a2b";
  TestGetFromCache(res_id, md5, base::PLATFORM_FILE_ERROR_NOT_FOUND);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataFileSystemTest, RemoveFromCache) {
  // Use alphanumeric characters for resource id.
  std::string res_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  // First store a file to cache.
  TestStoreToCache(res_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK);

  // Then try to remove existing file from cache.
  num_callback_invocations_ = 0;
  TestRemoveFromCache(res_id, base::PLATFORM_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);

  // Repeat using non-alphanumeric characters for resource id, including '.'
  // which is an extension separator.
  res_id = "pdf:`~!@#$%^&*()-_=+[{|]}\\;',<.>/?";
  TestStoreToCache(res_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK);

  num_callback_invocations_ = 0;
  TestRemoveFromCache(res_id, base::PLATFORM_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataFileSystemTest, PinAndUnpin) {
  std::string res_id("pdf:1a2b");
  std::string md5("abcdef0123456789");
  // First store a file to cache.
  TestStoreToCache(res_id, md5, GetTestFilePath("root_feed.json"),
                   base::PLATFORM_FILE_OK);

  // Pin the existing file in cache.
  num_callback_invocations_ = 0;
  TestPin(res_id, md5, base::PLATFORM_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);

  // Unpin the existing file in cache.
  num_callback_invocations_ = 0;
  TestUnpin(res_id, md5, base::PLATFORM_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);

  // Pin back the same existing file in cache.
  num_callback_invocations_ = 0;
  TestPin(res_id, md5, base::PLATFORM_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);

  // Pin a non-existent file in cache.
  res_id = "document:1a2b";
  num_callback_invocations_ = 0;
  TestPin(res_id, md5, base::PLATFORM_FILE_ERROR_NOT_FOUND);
  EXPECT_EQ(1, num_callback_invocations_);

  // Unpin a non-existent file in cache.
  num_callback_invocations_ = 0;
  TestUnpin(res_id, md5, base::PLATFORM_FILE_ERROR_NOT_FOUND);
  EXPECT_EQ(1, num_callback_invocations_);
}

TEST_F(GDataFileSystemTest, GetCacheState) {
  // Populate gdata file system.
  LoadRootFeedDocument("root_feed.json");
  LoadSubdirFeedDocument(FilePath(FILE_PATH_LITERAL("gdata/Directory 1")),
                         "subdir_feed.json");

  {  // Test cache state of an existing normal file.
    // Retrieve resource id and md5 of a file from file system.
    FilePath file_path(FILE_PATH_LITERAL("gdata/File 1.txt"));
    GDataFileBase* file_base = FindFile(file_path);
    ASSERT_TRUE(file_base != NULL);
    GDataFile* file = file_base->AsGDataFile();
    ASSERT_TRUE(file != NULL);
    std::string res_id = file->resource_id();
    std::string md5 = file->file_md5();

    // Store a file corresponding to |res_id| and |md5| to cache.
    TestStoreToCache(res_id, md5, GetTestFilePath("root_feed.json"),
                     base::PLATFORM_FILE_OK);

    // Get its cache state.
    num_callback_invocations_ = 0;
    TestGetCacheState(res_id, md5, base::PLATFORM_FILE_OK,
                      GDataFile::CACHE_STATE_PRESENT, file);
    EXPECT_EQ(1, num_callback_invocations_);
  }

  {  // Test cache state of an existing pinned file.
    // Retrieve resource id and md5 of a file from file system.
    FilePath file_path(
        FILE_PATH_LITERAL("gdata/Directory 1/SubDirectory File 1.txt"));
    GDataFileBase* file_base = FindFile(file_path);
    ASSERT_TRUE(file_base != NULL);
    GDataFile* file = file_base->AsGDataFile();
    ASSERT_TRUE(file != NULL);
    std::string res_id = file->resource_id();
    std::string md5 = file->file_md5();

    // Store a file corresponding to |res_id| and |md5| to cache, and pin
    // it.
    TestStoreToCache(res_id, md5, GetTestFilePath("root_feed.json"),
                     base::PLATFORM_FILE_OK);
    TestPin(res_id, md5, base::PLATFORM_FILE_OK);

    // Get its cache state.
    num_callback_invocations_ = 0;
    TestGetCacheState(res_id, md5, base::PLATFORM_FILE_OK,
                      GDataFile::CACHE_STATE_PRESENT |
                      GDataFile::CACHE_STATE_PINNED, file);
    EXPECT_EQ(1, num_callback_invocations_);
  }

  {  // Test cache state of a non-existent file.
    num_callback_invocations_ = 0;
    TestGetCacheState("pdf:12345", "abcd", base::PLATFORM_FILE_ERROR_NOT_FOUND,
                      0, NULL);
    EXPECT_EQ(1, num_callback_invocations_);
  }
}

TEST_F(GDataFileSystemTest, InitializeCache) {
  PrepareForInitCacheTest();
  TestInitializeCache();
}

// TODO(satorux): Write a test for GetFile() once DocumentsService is
// mockable.

TEST_F(GDataFileSystemTest, GetGDataFileInfoFromPath) {
  LoadRootFeedDocument("root_feed.json");

  // Lock to call GetGDataFileInfoFromPath.
  base::AutoLock lock(file_system_->lock_);
  GDataFileBase* file_info = file_system_->GetGDataFileInfoFromPath(
      FilePath(FILE_PATH_LITERAL("gdata/File 1.txt")));
  ASSERT_TRUE(file_info != NULL);
  EXPECT_EQ("https://file1_link_self/", file_info->self_url().spec());
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
  num_callback_invocations_ = 0;
  TestGetFromCacheForPath(gdata_file_path, base::PLATFORM_FILE_ERROR_NOT_FOUND);
  EXPECT_EQ(1, num_callback_invocations_);

  // Store a file corresponding to resource and md5 of "gdata/File 1.txt" to
  // cache.
  num_callback_invocations_ = 0;
  TestStoreToCache(file->resource_id(), file->file_md5(),
                   GetTestFilePath("root_feed.json"), base::PLATFORM_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);

  // Now the file should exist in cache.
  num_callback_invocations_ = 0;
  TestGetFromCacheForPath(gdata_file_path, base::PLATFORM_FILE_OK);
  EXPECT_EQ(1, num_callback_invocations_);

  // A file that doesn't exist in gdata.
  num_callback_invocations_ = 0;
  TestGetFromCacheForPath(FilePath(FILE_PATH_LITERAL("gdata/Nonexistent.txt")),
                          base::PLATFORM_FILE_ERROR_NOT_FOUND);
  EXPECT_EQ(1, num_callback_invocations_);
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
              DownloadFile(_, GURL("https://file_content_url/"), _));

  FilePath file_in_root(FILE_PATH_LITERAL("gdata/File 1.txt"));
  file_system_->GetFile(file_in_root, callback);
  message_loop_.RunAllPending();  // Wait to get our result.
  EXPECT_STREQ("file_content_url/",
               callback_helper_->download_path_.value().c_str());
}

TEST_F(GDataFileSystemTest, GetAvailableSpace) {
  GDataFileSystem::GetAvailableSpaceCallback callback =
      base::Bind(&CallbackHelper::GetAvailableSpaceCallback,
                 callback_helper_.get());

  EXPECT_CALL(*mock_doc_service_, GetAccountMetadata(_));

  file_system_->GetAvailableSpace(callback);
  message_loop_.RunAllPending();  // Wait to get our result.
  EXPECT_EQ(1234, callback_helper_->quota_bytes_used_);
  EXPECT_EQ(12345, callback_helper_->quota_bytes_total_);
}

}   // namespace gdata
