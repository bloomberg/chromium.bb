// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#include <string>
#include <vector>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/gdata/gdata_file_system.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/test/base/testing_profile.h"
#include "content/test/test_browser_thread.h"
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

class GDataFileSystemTest : public testing::Test {
 protected:
  GDataFileSystemTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {
  }

  virtual void SetUp() {
    profile_.reset(new TestingProfile);
    file_system_ = GDataFileSystemFactory::GetForProfile(profile_.get());
  }

  // Loads test json file as root ("/gdata") element.
  void LoadRootFeedDocument(const std::string& filename) {
    LoadSubdirFeedDocument(FilePath("gdata"), filename);
  }

  // Loads test json file as subdirectory content of |directory_path|.
  void LoadSubdirFeedDocument(const FilePath& directory_path,
                              const std::string& filename) {
    std::string error;
    scoped_ptr<Value> document(LoadJSONFile(filename));
    ASSERT_TRUE(document.get());
    ASSERT_TRUE(document->GetType() == Value::TYPE_DICTIONARY);
    GURL unused;
    ASSERT_TRUE(UpdateContent(directory_path, document.get()));
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
                     Value* value) {
    GURL unused;
    return file_system_->UpdateDirectoryWithDocumentFeed(
        directory_path,
        GURL(),   // feed_url
        value,
        true,     // is_initial_feed
        &unused) == base::PLATFORM_FILE_OK;
  }

  void UpdateSubdirContent(const FilePath& search_file_path,
                           const FilePath& directory_path,
                           scoped_refptr<FindFileDelegate> delegate,
                           Value* data) {
    GDataFileSystem::FindFileParams params(
        search_file_path,
        false,
        directory_path,
        GURL(),
        true,
        delegate);
    file_system_->OnGetDocuments(params,
                                 HTTP_SUCCESS,
                                 data);
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
    scoped_refptr<ReadOnlyFindFileDelegate> search_delegate(
        new ReadOnlyFindFileDelegate());
    file_system_->FindFileByPath(file_path,
                                 search_delegate);
    GDataFileBase* file = FindFile(file_path);
    ASSERT_TRUE(file);
    EXPECT_EQ(file->GetFilePath(), file_path);
  }

  static Value* LoadJSONFile(const std::string& filename) {
    FilePath path;
    std::string error;
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    path = path.AppendASCII("chromeos")
        .AppendASCII("gdata")
        .AppendASCII(filename.c_str());
    EXPECT_TRUE(file_util::PathExists(path)) <<
        "Couldn't find " << path.value();

    JSONFileValueSerializer serializer(path);
    Value* value = serializer.Deserialize(NULL, &error);
    EXPECT_TRUE(value) <<
        "Parse error " << path.value() << ": " << error;
    return value;
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  scoped_ptr<TestingProfile> profile_;
  GDataFileSystem* file_system_;
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
              OnDirectoryFound(FilePath("gdata"), _))
      .Times(1);

  file_system_->FindFileByPath(FilePath("gdata"),
                               mock_find_file_delegate);
}

TEST_F(GDataFileSystemTest, SearchExistingFile) {
  LoadRootFeedDocument("root_feed.json");
  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate =
      new MockFindFileDelegate();

  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath("gdata"), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));
  EXPECT_CALL(*mock_find_file_delegate.get(), OnFileFound(_))
      .Times(1);

  file_system_->FindFileByPath(FilePath("gdata/File 1.txt"),
                               mock_find_file_delegate);
}

TEST_F(GDataFileSystemTest, SearchExistingDocument) {
  LoadRootFeedDocument("root_feed.json");
  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate =
      new MockFindFileDelegate();

  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath("gdata"), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));
  EXPECT_CALL(*mock_find_file_delegate.get(), OnFileFound(_))
      .Times(1);

  file_system_->FindFileByPath(FilePath("gdata/Document 1.gdocument"),
                               mock_find_file_delegate);
}

TEST_F(GDataFileSystemTest, SearchDuplicateNames) {
  LoadRootFeedDocument("root_feed.json");

  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate =
      new MockFindFileDelegate();
  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath("gdata"), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));
  EXPECT_CALL(*mock_find_file_delegate.get(), OnFileFound(_))
      .Times(1);
  file_system_->FindFileByPath(FilePath("gdata/Duplicate Name.txt"),
                               mock_find_file_delegate);

  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate2 =
      new MockFindFileDelegate();
  EXPECT_CALL(*mock_find_file_delegate2.get(),
              OnEnterDirectory(FilePath("gdata"), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));
  EXPECT_CALL(*mock_find_file_delegate2.get(), OnFileFound(_))
      .Times(1);
  file_system_->FindFileByPath(FilePath("gdata/Duplicate Name (2).txt"),
                               mock_find_file_delegate2);
}

TEST_F(GDataFileSystemTest, SearchExistingDirectory) {
  LoadRootFeedDocument("root_feed.json");
  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate =
      new MockFindFileDelegate();

  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath("gdata"), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));
  EXPECT_CALL(*mock_find_file_delegate.get(), OnDirectoryFound(_, _))
      .Times(1);

  file_system_->FindFileByPath(FilePath("gdata/Directory 1"),
                               mock_find_file_delegate);
}


TEST_F(GDataFileSystemTest, SearchNonExistingFile) {
  LoadRootFeedDocument("root_feed.json");
  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate =
      new MockFindFileDelegate();

  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath("gdata"), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));
  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnError(base::PLATFORM_FILE_ERROR_NOT_FOUND))
      .Times(1);

  file_system_->FindFileByPath(FilePath("gdata/nonexisting.file"),
                               mock_find_file_delegate);
}

TEST_F(GDataFileSystemTest, StopFileSearch) {
  LoadRootFeedDocument("root_feed.json");
  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate =
      new MockFindFileDelegate();

  // Stop on first directory entry.
  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath("gdata"), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_TERMINATES));

  file_system_->FindFileByPath(FilePath("gdata/Directory 1"),
                               mock_find_file_delegate);
}

TEST_F(GDataFileSystemTest, SearchInSubdir) {
  LoadRootFeedDocument("root_feed.json");
  LoadSubdirFeedDocument(FilePath("gdata/Directory 1"), "subdir_feed.json");

  scoped_refptr<MockFindFileDelegate> mock_find_file_delegate =
      new MockFindFileDelegate();

  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath("gdata"), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));

  EXPECT_CALL(*mock_find_file_delegate.get(),
              OnEnterDirectory(FilePath("gdata/Directory 1"), _))
      .Times(1)
      .WillOnce(Return(FindFileDelegate::FIND_FILE_CONTINUES));

  EXPECT_CALL(*mock_find_file_delegate.get(), OnFileFound(_))
      .Times(1);

  file_system_->FindFileByPath(
      FilePath("gdata/Directory 1/SubDirectory File 1.txt"),
      mock_find_file_delegate);
}

TEST_F(GDataFileSystemTest, FilePathTests) {
  LoadRootFeedDocument("root_feed.json");
  LoadSubdirFeedDocument(FilePath("gdata/Directory 1"), "subdir_feed.json");

  FindAndTestFilePath(FilePath("gdata/File 1.txt"));
  FindAndTestFilePath(FilePath("gdata/Directory 1"));
  FindAndTestFilePath(FilePath("gdata/Directory 1/SubDirectory File 1.txt"));
}

TEST_F(GDataFileSystemTest, RemoveFiles) {
  LoadRootFeedDocument("root_feed.json");
  LoadSubdirFeedDocument(FilePath("gdata/Directory 1"), "subdir_feed.json");

  FilePath nonexisting_file("gdata/Dummy file.txt");
  FilePath file_in_root("gdata/File 1.txt");
  FilePath dir_in_root("gdata/Directory 1");
  FilePath file_in_subdir("gdata/Directory 1/SubDirectory File 1.txt");

  EXPECT_TRUE(FindFile(file_in_root) != NULL);
  EXPECT_TRUE(FindFile(dir_in_root) != NULL);
  EXPECT_TRUE(FindFile(file_in_subdir) != NULL);

  // Remove first file in root.
  EXPECT_TRUE(RemoveFile(file_in_root));
  EXPECT_TRUE(FindFile(file_in_root) == NULL);
  EXPECT_TRUE(FindFile(dir_in_root) != NULL);
  EXPECT_TRUE(FindFile(file_in_subdir) != NULL);

  // Remove directory.
  EXPECT_TRUE(RemoveFile(dir_in_root));
  EXPECT_TRUE(FindFile(file_in_root) == NULL);
  EXPECT_TRUE(FindFile(dir_in_root) == NULL);
  EXPECT_TRUE(FindFile(file_in_subdir) == NULL);

  // Try removing file in already removed subdirectory.
  EXPECT_FALSE(RemoveFile(file_in_subdir));

  // Try removing non-existing file.
  EXPECT_FALSE(RemoveFile(nonexisting_file));

  // Try removing root file element.
  EXPECT_FALSE(RemoveFile(FilePath("gdata")));
}


TEST_F(GDataFileSystemTest, CreateDirectory) {
  LoadRootFeedDocument("root_feed.json");
  LoadSubdirFeedDocument(FilePath("gdata/Directory 1"), "subdir_feed.json");

  // Create directory in root.
  FilePath dir_path("gdata/New Folder 1");
  EXPECT_TRUE(FindFile(dir_path) == NULL);
  AddDirectoryFromFile(dir_path, "directory_entry_atom.json");
  EXPECT_TRUE(FindFile(dir_path) != NULL);

  // Create directory in a sub dirrectory.
  FilePath subdir_path("gdata/New Folder 1/New Folder 2");
  EXPECT_TRUE(FindFile(subdir_path) == NULL);
  AddDirectoryFromFile(subdir_path, "directory_entry_atom.json");
  EXPECT_TRUE(FindFile(subdir_path) != NULL);
}

TEST_F(GDataFileSystemTest, FindFirstMissingParentDirectory) {
  LoadRootFeedDocument("root_feed.json");
  LoadSubdirFeedDocument(FilePath("gdata/Directory 1"), "subdir_feed.json");

  GURL last_dir_content_url;
  FilePath first_missing_parent_path;

  // Create directory in root.
  FilePath dir_path("gdata/New Folder 1");
  EXPECT_EQ(
      file_system_->FindFirstMissingParentDirectory(dir_path,
          &last_dir_content_url,
          &first_missing_parent_path),
      GDataFileSystem::FOUND_MISSING);
  EXPECT_EQ(dir_path, first_missing_parent_path);
  EXPECT_TRUE(last_dir_content_url.is_empty());    // root directory.

  // Missing folders in subdir of an existing folder.
  FilePath dir_path2("gdata/Directory 1/New Folder 2");
  EXPECT_EQ(
      file_system_->FindFirstMissingParentDirectory(dir_path2,
          &last_dir_content_url,
          &first_missing_parent_path),
      GDataFileSystem::FOUND_MISSING);
  EXPECT_EQ(dir_path2, first_missing_parent_path);
  EXPECT_FALSE(last_dir_content_url.is_empty());    // non-root directory.

  // Missing two folders on the path.
  FilePath dir_path3 = dir_path2.Append("Another Foder");
  EXPECT_EQ(
      file_system_->FindFirstMissingParentDirectory(dir_path3,
          &last_dir_content_url,
          &first_missing_parent_path),
      GDataFileSystem::FOUND_MISSING);
  EXPECT_EQ(dir_path3.DirName(), first_missing_parent_path);
  EXPECT_FALSE(last_dir_content_url.is_empty());    // non-root directory.

  // Folders on top of an existing file.
  EXPECT_EQ(
      file_system_->FindFirstMissingParentDirectory(
          FilePath("gdata/File 1.txt/BadDir"),
          &last_dir_content_url,
          &first_missing_parent_path),
      GDataFileSystem::FOUND_INVALID);

  // Existing folder.
  EXPECT_EQ(
      file_system_->FindFirstMissingParentDirectory(
          FilePath("gdata/Directory 1"),
          &last_dir_content_url,
          &first_missing_parent_path),
      GDataFileSystem::DIRECTORY_ALREADY_PRESENT);
}

}   // namespace gdata
