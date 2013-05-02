// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/search_metadata.h"

#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/threading/sequenced_worker_pool.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {

namespace {

const int kDefaultAtMostNumMatches = 10;

}  // namespace

class SearchMetadataTest : public testing::Test {
 protected:
  SearchMetadataTest()
      : ui_thread_(content::BrowserThread::UI, &message_loop_) {
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());

    scoped_refptr<base::SequencedWorkerPool> pool =
        content::BrowserThread::GetBlockingPool();
    blocking_task_runner_ =
        pool->GetSequencedTaskRunner(pool->GetSequenceToken());

    resource_metadata_.reset(new internal::ResourceMetadata(
        temp_dir_.path(),
        blocking_task_runner_));

    FileError error = FILE_ERROR_FAILED;
    resource_metadata_->Initialize(
        google_apis::test_util::CreateCopyResultCallback(&error));
    google_apis::test_util::RunBlockingPoolTask();
    ASSERT_EQ(FILE_ERROR_OK, error);

    AddEntriesToMetadata();
  }

  void AddEntriesToMetadata() {
    ResourceEntry entry;

    AddEntryToMetadata(GetDirectoryEntry(
        util::kDriveMyDriveRootDirName, "root", 100,
        util::kDriveGrandRootSpecialResourceId));

    AddEntryToMetadata(GetDirectoryEntry(
        "Directory 1", "dir1", 1, "root"));
    AddEntryToMetadata(GetFileEntry(
        "SubDirectory File 1.txt", "file1a", 2, "dir1"));

    entry = GetFileEntry(
        "Shared To The Account Owner.txt", "file1b", 3, "dir1");
    entry.set_shared_with_me(true);
    AddEntryToMetadata(entry);

    AddEntryToMetadata(GetDirectoryEntry(
        "Directory 2 excludeDir-test", "dir2", 4, "root"));

    AddEntryToMetadata(GetDirectoryEntry(
        "Slash \xE2\x88\x95 in directory", "dir3", 5, "root"));
    AddEntryToMetadata(GetFileEntry(
        "Slash SubDir File.txt", "file3a", 6, "dir3"));

    entry = GetFileEntry(
        "Document 1 excludeDir-test", "doc1", 7, "root");
    entry.mutable_file_specific_info()->set_is_hosted_document(true);
    entry.mutable_file_specific_info()->set_document_extension(".gdoc");
    AddEntryToMetadata(entry);
  }

  ResourceEntry GetFileEntry(const std::string& name,
                               const std::string& resource_id,
                               int64 last_accessed,
                               const std::string& parent_resource_id) {
    ResourceEntry entry;
    entry.set_title(name);
    entry.set_resource_id(resource_id);
    entry.set_parent_resource_id(parent_resource_id);
    entry.mutable_file_info()->set_last_accessed(last_accessed);
    return entry;
  }

  ResourceEntry GetDirectoryEntry(const std::string& name,
                                    const std::string& resource_id,
                                    int64 last_accessed,
                                    const std::string& parent_resource_id) {
    ResourceEntry entry;
    entry.set_title(name);
    entry.set_resource_id(resource_id);
    entry.set_parent_resource_id(parent_resource_id);
    entry.mutable_file_info()->set_last_accessed(last_accessed);
    entry.mutable_file_info()->set_is_directory(true);
    return entry;
  }

  void AddEntryToMetadata(const ResourceEntry& entry) {
    FileError error = FILE_ERROR_FAILED;
    base::FilePath drive_path;

    resource_metadata_->AddEntry(
        entry,
        google_apis::test_util::CreateCopyResultCallback(&error, &drive_path));
    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(FILE_ERROR_OK, error);
  }

  MessageLoopForUI message_loop_;
  content::TestBrowserThread ui_thread_;
  base::ScopedTempDir temp_dir_;
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  scoped_ptr<internal::ResourceMetadata, test_util::DestroyHelperForTests>
      resource_metadata_;
};

TEST_F(SearchMetadataTest, SearchMetadata_ZeroMatches) {
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(resource_metadata_.get(),
                 "NonExistent",
                 SEARCH_METADATA_ALL,
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_EQ(0U, result->size());
}

TEST_F(SearchMetadataTest, SearchMetadata_RegularFile) {
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(resource_metadata_.get(),
                 "SubDirectory File 1.txt",
                 SEARCH_METADATA_ALL,
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_EQ(1U, result->size());
  EXPECT_EQ("drive/root/Directory 1/SubDirectory File 1.txt",
            result->at(0).path.AsUTF8Unsafe());
}

// This test checks if |FindAndHighlight| does case-insensitive search.
// Tricker test cases for |FindAndHighlight| can be found below.
TEST_F(SearchMetadataTest, SearchMetadata_CaseInsensitiveSearch) {
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  // The query is all in lower case.
  SearchMetadata(resource_metadata_.get(),
                 "subdirectory file 1.txt",
                 SEARCH_METADATA_ALL,
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_EQ(1U, result->size());
  EXPECT_EQ("drive/root/Directory 1/SubDirectory File 1.txt",
            result->at(0).path.AsUTF8Unsafe());
}

TEST_F(SearchMetadataTest, SearchMetadata_RegularFiles) {
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(resource_metadata_.get(),
                 "SubDir",
                 SEARCH_METADATA_ALL,
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_EQ(2U, result->size());

  // The results should be sorted by the last accessed time in descending order.
  EXPECT_EQ(6, result->at(0).entry.file_info().last_accessed());
  EXPECT_EQ(2, result->at(1).entry.file_info().last_accessed());

  // All base names should contain "File".
  EXPECT_EQ("drive/root/Slash \xE2\x88\x95 in directory/Slash SubDir File.txt",
            result->at(0).path.AsUTF8Unsafe());
  EXPECT_EQ("drive/root/Directory 1/SubDirectory File 1.txt",
            result->at(1).path.AsUTF8Unsafe());
}

TEST_F(SearchMetadataTest, SearchMetadata_AtMostOneFile) {
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  // There are two files matching "SubDir" but only one file should be
  // returned.
  SearchMetadata(resource_metadata_.get(),
                 "SubDir",
                 SEARCH_METADATA_ALL,
                 1,  // at_most_num_matches
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_EQ(1U, result->size());
  EXPECT_EQ("drive/root/Slash \xE2\x88\x95 in directory/Slash SubDir File.txt",
            result->at(0).path.AsUTF8Unsafe());
}

TEST_F(SearchMetadataTest, SearchMetadata_Directory) {
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(resource_metadata_.get(),
                 "Directory 1",
                 SEARCH_METADATA_ALL,
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_EQ(1U, result->size());
  EXPECT_EQ("drive/root/Directory 1", result->at(0).path.AsUTF8Unsafe());
}

TEST_F(SearchMetadataTest, SearchMetadata_HostedDocument) {
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(resource_metadata_.get(),
                 "Document",
                 SEARCH_METADATA_ALL,
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_EQ(1U, result->size());

  EXPECT_EQ("drive/root/Document 1 excludeDir-test.gdoc",
            result->at(0).path.AsUTF8Unsafe());
}

TEST_F(SearchMetadataTest, SearchMetadata_ExcludeHostedDocument) {
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(resource_metadata_.get(),
                 "Document",
                 SEARCH_METADATA_EXCLUDE_HOSTED_DOCUMENTS,
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_EQ(0U, result->size());
}

TEST_F(SearchMetadataTest, SearchMetadata_SharedWithMe) {
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(resource_metadata_.get(),
                 "",
                 SEARCH_METADATA_SHARED_WITH_ME,
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_EQ(1U, result->size());
  EXPECT_EQ("drive/root/Directory 1/Shared To The Account Owner.txt",
            result->at(0).path.AsUTF8Unsafe());
}

TEST_F(SearchMetadataTest, SearchMetadata_FileAndDirectory) {
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(resource_metadata_.get(),
                 "excludeDir-test",
                 SEARCH_METADATA_ALL,
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));

  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_EQ(2U, result->size());

  EXPECT_EQ("drive/root/Document 1 excludeDir-test.gdoc",
            result->at(0).path.AsUTF8Unsafe());
  EXPECT_EQ("drive/root/Directory 2 excludeDir-test",
            result->at(1).path.AsUTF8Unsafe());
}

TEST_F(SearchMetadataTest, SearchMetadata_ExcludeDirectory) {
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(resource_metadata_.get(),
                 "excludeDir-test",
                 SEARCH_METADATA_EXCLUDE_DIRECTORIES,
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));

  google_apis::test_util::RunBlockingPoolTask();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_EQ(1U, result->size());

  EXPECT_EQ("drive/root/Document 1 excludeDir-test.gdoc",
            result->at(0).path.AsUTF8Unsafe());
}

// "drive", "drive/root", "drive/other" should be excluded.
TEST_F(SearchMetadataTest, SearchMetadata_ExcludeSpecialDirectories) {
  const char* kQueries[] = { "drive", "root", "other" };
  for (size_t i = 0; i < arraysize(kQueries); ++i) {
    FileError error = FILE_ERROR_FAILED;
    scoped_ptr<MetadataSearchResultVector> result;

    const std::string query = kQueries[i];
    SearchMetadata(resource_metadata_.get(),
                   query,
                   SEARCH_METADATA_ALL,
                   kDefaultAtMostNumMatches,
                   google_apis::test_util::CreateCopyResultCallback(
                       &error, &result));

    google_apis::test_util::RunBlockingPoolTask();
    EXPECT_EQ(FILE_ERROR_OK, error);
    ASSERT_TRUE(result->empty()) << ": " << query << " should not match";
  }
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_ZeroMatches) {
  std::string highlighted_text;
  EXPECT_FALSE(FindAndHighlight("text", "query", &highlighted_text));
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_EmptyQuery) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlight("text", "", &highlighted_text));
  EXPECT_EQ("", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_EmptyText) {
  std::string highlighted_text;
  EXPECT_FALSE(FindAndHighlight("", "query", &highlighted_text));
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_EmptyTextAndQuery) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlight("", "", &highlighted_text));
  EXPECT_EQ("", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_FullMatch) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlight("hello", "hello", &highlighted_text));
  EXPECT_EQ("<b>hello</b>", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_StartWith) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlight("hello, world", "hello", &highlighted_text));
  EXPECT_EQ("<b>hello</b>, world", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_EndWith) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlight("hello, world", "world", &highlighted_text));
  EXPECT_EQ("hello, <b>world</b>", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_InTheMiddle) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlight("yo hello, world", "hello", &highlighted_text));
  EXPECT_EQ("yo <b>hello</b>, world", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_MultipeMatches) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlight("yoyoyoyoy", "yoy", &highlighted_text));
  EXPECT_EQ("<b>yoy</b>o<b>yoy</b>oy", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_IgnoreCase) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlight("HeLLo", "hello", &highlighted_text));
  EXPECT_EQ("<b>HeLLo</b>", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_MetaChars) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlight("<hello>", "hello", &highlighted_text));
  EXPECT_EQ("&lt;<b>hello</b>&gt;", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_MoreMetaChars) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlight("a&b&c&d", "b&c", &highlighted_text));
  EXPECT_EQ("a&amp;<b>b&amp;c</b>&amp;d", highlighted_text);
}

}   // namespace drive
