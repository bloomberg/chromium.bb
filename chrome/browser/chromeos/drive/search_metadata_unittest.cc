// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/drive/search_metadata.h"

#include "base/files/scoped_temp_dir.h"
#include "base/i18n/string_search.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/chromeos/drive/fake_free_disk_space_getter.h"
#include "chrome/browser/chromeos/drive/file_cache.h"
#include "chrome/browser/chromeos/drive/file_system_util.h"
#include "chrome/browser/chromeos/drive/test_util.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace drive {
namespace internal {

namespace {

const int kDefaultAtMostNumMatches = 10;
const int64 kCacheEntriesLastAccessedTimeBase = 100;

// A simple wrapper for testing FindAndHighlightWrapper(). It just converts the
// query text parameter to FixedPatternStringSearchIgnoringCaseAndAccents.
bool FindAndHighlightWrapper(
    const std::string& text,
    const std::string& query_text,
    std::string* highlighted_text) {
  base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents query(
      base::UTF8ToUTF16(query_text));
  return FindAndHighlight(text, &query, highlighted_text);
}

// Generator of sequential fake data for ResourceEntry.
class MetadataInfoGenerator {
 public:
  // Constructor of EntryInfoGenerator. |prefix| is prefix of resource IDs and
  // |last_accessed_base| is the first value to be generated as a last accessed
  // time.
  MetadataInfoGenerator(const std::string& prefix,
                        int last_accessed_base) :
      prefix_(prefix),
      id_counter_(0),
      last_accessed_counter_(last_accessed_base) {}

  // Obtains resource ID that is consists of the prefix and a sequential
  // number.
  std::string GetId() const {
    return base::StringPrintf("%s%d", prefix_.c_str(), id_counter_);
  }

  // Obtains the fake last accessed time that is sequential number following
  // |last_accessed_base| specified at the constructor.
  int64 GetLastAccessed() const {
    return last_accessed_counter_;
  }

  // Advances counters to generate the next ID and last accessed time.
  void Advance() {
    ++id_counter_;
    ++last_accessed_counter_;
  }

 private:
  std::string prefix_;
  int id_counter_;
  int64 last_accessed_counter_;
};

}  // namespace

class SearchMetadataTest : public testing::Test {
 protected:
  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(temp_dir_.CreateUniqueTempDir());
    fake_free_disk_space_getter_.reset(new FakeFreeDiskSpaceGetter);

    metadata_storage_.reset(new ResourceMetadataStorage(
        temp_dir_.path(), base::MessageLoopProxy::current().get()));
    ASSERT_TRUE(metadata_storage_->Initialize());

    cache_.reset(new FileCache(metadata_storage_.get(),
                               temp_dir_.path(),
                               base::MessageLoopProxy::current().get(),
                               fake_free_disk_space_getter_.get()));
    ASSERT_TRUE(cache_->Initialize());

    resource_metadata_.reset(
        new ResourceMetadata(metadata_storage_.get(),
                             base::MessageLoopProxy::current()));
    ASSERT_EQ(FILE_ERROR_OK, resource_metadata_->Initialize());

    AddEntriesToMetadata();
  }

  void AddEntriesToMetadata() {
    ResourceEntry entry;
    std::string local_id;

    EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(GetDirectoryEntry(
        util::kDriveMyDriveRootDirName, "root", 100,
        util::kDriveGrandRootSpecialResourceId), &local_id));

    EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(GetDirectoryEntry(
        "Directory 1", "dir1", 1, "root"), &local_id));
    EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(GetFileEntry(
        "SubDirectory File 1.txt", "file1a", 2, "dir1"), &local_id));

    entry = GetFileEntry(
        "Shared To The Account Owner.txt", "file1b", 3, "dir1");
    entry.set_shared_with_me(true);
    EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(entry, &local_id));

    EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(GetDirectoryEntry(
        "Directory 2 excludeDir-test", "dir2", 4, "root"), &local_id));

    EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(GetDirectoryEntry(
        "Slash \xE2\x88\x95 in directory", "dir3", 5, "root"), &local_id));
    EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(GetFileEntry(
        "Slash SubDir File.txt", "file3a", 6, "dir3"), &local_id));

    entry = GetFileEntry(
        "Document 1 excludeDir-test", "doc1", 7, "root");
    entry.mutable_file_specific_info()->set_is_hosted_document(true);
    entry.mutable_file_specific_info()->set_document_extension(".gdoc");
    EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(entry, &local_id));
  }

  // Adds a directory at |path|. Parent directories are added if needed just
  // like "mkdir -p" does.
  std::string AddDirectoryToMetadataWithParents(
      const base::FilePath& path,
      MetadataInfoGenerator* generator) {
    if (path == base::FilePath(base::FilePath::kCurrentDirectory))
      return "root";

    {
      ResourceEntry entry;
      FileError error = resource_metadata_->GetResourceEntryByPath(
          util::GetDriveMyDriveRootPath().Append(path), &entry);
      if (error == FILE_ERROR_OK)
        return entry.resource_id();
    }

    const std::string parent_id =
        AddDirectoryToMetadataWithParents(path.DirName(), generator);
    const std::string id = generator->GetId();
    std::string local_id;
    EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
        GetDirectoryEntry(path.BaseName().AsUTF8Unsafe(),
                          id,
                          generator->GetLastAccessed(),
                          parent_id), &local_id));
    generator->Advance();
    return local_id;
  }

  // Adds entries for |cache_resources| to |resource_metadata_|.  The parent
  // directories of |resources| is also added.
  void AddEntriesToMetadataFromCache(
      const std::vector<test_util::TestCacheResource>& cache_resources,
      MetadataInfoGenerator* generator) {
    for (size_t i = 0; i < cache_resources.size(); ++i) {
      const test_util::TestCacheResource& resource = cache_resources[i];
      const base::FilePath path(resource.source_file);
      const std::string parent_id =
          AddDirectoryToMetadataWithParents(path.DirName(), generator);
      std::string local_id;
      EXPECT_EQ(FILE_ERROR_OK, resource_metadata_->AddEntry(
          GetFileEntry(path.BaseName().AsUTF8Unsafe(),
                       resource.resource_id,
                       generator->GetLastAccessed(),
                       parent_id), &local_id));
      generator->Advance();
    }
  }

  ResourceEntry GetFileEntry(const std::string& name,
                               const std::string& resource_id,
                               int64 last_accessed,
                               const std::string& parent_local_id) {
    ResourceEntry entry;
    entry.set_title(name);
    entry.set_resource_id(resource_id);
    entry.set_parent_local_id(parent_local_id);
    entry.mutable_file_info()->set_last_accessed(last_accessed);
    return entry;
  }

  ResourceEntry GetDirectoryEntry(const std::string& name,
                                    const std::string& resource_id,
                                    int64 last_accessed,
                                    const std::string& parent_local_id) {
    ResourceEntry entry;
    entry.set_title(name);
    entry.set_resource_id(resource_id);
    entry.set_parent_local_id(parent_local_id);
    entry.mutable_file_info()->set_last_accessed(last_accessed);
    entry.mutable_file_info()->set_is_directory(true);
    return entry;
  }

  content::TestBrowserThreadBundle thread_bundle_;
  base::ScopedTempDir temp_dir_;
  scoped_ptr<FakeFreeDiskSpaceGetter> fake_free_disk_space_getter_;
  scoped_ptr<ResourceMetadataStorage,
             test_util::DestroyHelperForTests> metadata_storage_;
  scoped_ptr<ResourceMetadata, test_util::DestroyHelperForTests>
      resource_metadata_;
  scoped_ptr<FileCache, test_util::DestroyHelperForTests> cache_;
};

TEST_F(SearchMetadataTest, SearchMetadata_ZeroMatches) {
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(base::MessageLoopProxy::current(),
                 resource_metadata_.get(),
                 "NonExistent",
                 SEARCH_METADATA_ALL,
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(result);
  ASSERT_EQ(0U, result->size());
}

TEST_F(SearchMetadataTest, SearchMetadata_RegularFile) {
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(base::MessageLoopProxy::current(),
                 resource_metadata_.get(),
                 "SubDirectory File 1.txt",
                 SEARCH_METADATA_ALL,
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(result);
  ASSERT_EQ(1U, result->size());
  EXPECT_EQ("drive/root/Directory 1/SubDirectory File 1.txt",
            result->at(0).path.AsUTF8Unsafe());
}

// This test checks if |FindAndHighlightWrapper| does case-insensitive search.
// Tricker test cases for |FindAndHighlightWrapper| can be found below.
TEST_F(SearchMetadataTest, SearchMetadata_CaseInsensitiveSearch) {
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  // The query is all in lower case.
  SearchMetadata(base::MessageLoopProxy::current(),
                 resource_metadata_.get(),
                 "subdirectory file 1.txt",
                 SEARCH_METADATA_ALL,
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(result);
  ASSERT_EQ(1U, result->size());
  EXPECT_EQ("drive/root/Directory 1/SubDirectory File 1.txt",
            result->at(0).path.AsUTF8Unsafe());
}

TEST_F(SearchMetadataTest, SearchMetadata_RegularFiles) {
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(base::MessageLoopProxy::current(),
                 resource_metadata_.get(),
                 "SubDir",
                 SEARCH_METADATA_ALL,
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(result);
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
  SearchMetadata(base::MessageLoopProxy::current(),
                 resource_metadata_.get(),
                 "SubDir",
                 SEARCH_METADATA_ALL,
                 1,  // at_most_num_matches
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(result);
  ASSERT_EQ(1U, result->size());
  EXPECT_EQ("drive/root/Slash \xE2\x88\x95 in directory/Slash SubDir File.txt",
            result->at(0).path.AsUTF8Unsafe());
}

TEST_F(SearchMetadataTest, SearchMetadata_Directory) {
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(base::MessageLoopProxy::current(),
                 resource_metadata_.get(),
                 "Directory 1",
                 SEARCH_METADATA_ALL,
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(result);
  ASSERT_EQ(1U, result->size());
  EXPECT_EQ("drive/root/Directory 1", result->at(0).path.AsUTF8Unsafe());
}

TEST_F(SearchMetadataTest, SearchMetadata_HostedDocument) {
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(base::MessageLoopProxy::current(),
                 resource_metadata_.get(),
                 "Document",
                 SEARCH_METADATA_ALL,
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(result);
  ASSERT_EQ(1U, result->size());

  EXPECT_EQ("drive/root/Document 1 excludeDir-test.gdoc",
            result->at(0).path.AsUTF8Unsafe());
}

TEST_F(SearchMetadataTest, SearchMetadata_ExcludeHostedDocument) {
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(base::MessageLoopProxy::current(),
                 resource_metadata_.get(),
                 "Document",
                 SEARCH_METADATA_EXCLUDE_HOSTED_DOCUMENTS,
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(result);
  ASSERT_EQ(0U, result->size());
}

TEST_F(SearchMetadataTest, SearchMetadata_SharedWithMe) {
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(base::MessageLoopProxy::current(),
                 resource_metadata_.get(),
                 "",
                 SEARCH_METADATA_SHARED_WITH_ME,
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(result);
  ASSERT_EQ(1U, result->size());
  EXPECT_EQ("drive/root/Directory 1/Shared To The Account Owner.txt",
            result->at(0).path.AsUTF8Unsafe());
}

TEST_F(SearchMetadataTest, SearchMetadata_FileAndDirectory) {
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(base::MessageLoopProxy::current(),
                 resource_metadata_.get(),
                 "excludeDir-test",
                 SEARCH_METADATA_ALL,
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(result);
  ASSERT_EQ(2U, result->size());

  EXPECT_EQ("drive/root/Document 1 excludeDir-test.gdoc",
            result->at(0).path.AsUTF8Unsafe());
  EXPECT_EQ("drive/root/Directory 2 excludeDir-test",
            result->at(1).path.AsUTF8Unsafe());
}

TEST_F(SearchMetadataTest, SearchMetadata_ExcludeDirectory) {
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(base::MessageLoopProxy::current(),
                 resource_metadata_.get(),
                 "excludeDir-test",
                 SEARCH_METADATA_EXCLUDE_DIRECTORIES,
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));

  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_TRUE(result);
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
    SearchMetadata(base::MessageLoopProxy::current(),
                   resource_metadata_.get(),
                   query,
                   SEARCH_METADATA_ALL,
                   kDefaultAtMostNumMatches,
                   google_apis::test_util::CreateCopyResultCallback(
                       &error, &result));

    base::RunLoop().RunUntilIdle();
    EXPECT_EQ(FILE_ERROR_OK, error);
    ASSERT_TRUE(result);
    ASSERT_TRUE(result->empty()) << ": " << query << " should not match";
  }
}

TEST_F(SearchMetadataTest, SearchMetadata_Offline) {
  const std::vector<test_util::TestCacheResource> cache_resources =
      test_util::GetDefaultTestCacheResources();
  ASSERT_TRUE(test_util::PrepareTestCacheResources(cache_.get(),
                                                   cache_resources));
  {
    MetadataInfoGenerator generator("cache", kCacheEntriesLastAccessedTimeBase);
    AddEntriesToMetadataFromCache(cache_resources, &generator);
  }
  FileError error = FILE_ERROR_FAILED;
  scoped_ptr<MetadataSearchResultVector> result;

  SearchMetadata(base::MessageLoopProxy::current(),
                 resource_metadata_.get(),
                 "",
                 SEARCH_METADATA_OFFLINE,
                 kDefaultAtMostNumMatches,
                 google_apis::test_util::CreateCopyResultCallback(
                     &error, &result));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(FILE_ERROR_OK, error);
  ASSERT_EQ(6U, result->size());

  // Newer entries are listed earlier. So, the results come in the reverse
  // order of addition written in test_util::GetDefaultTestCacheResources.
  EXPECT_EQ("drive/root/pinned/dirty/cache.pdf",
            result->at(0).path.AsUTF8Unsafe());
  EXPECT_EQ("drive/root/dirty/cache.avi",
            result->at(1).path.AsUTF8Unsafe());
  EXPECT_EQ("drive/root/pinned/cache.mp3",
            result->at(2).path.AsUTF8Unsafe());
  EXPECT_EQ("drive/root/cache2.png",
            result->at(3).path.AsUTF8Unsafe());
  EXPECT_EQ("drive/root/cache.txt",
            result->at(4).path.AsUTF8Unsafe());
  // This is not included in the cache but is a hosted document.
  EXPECT_EQ("drive/root/Document 1 excludeDir-test.gdoc",
            result->at(5).path.AsUTF8Unsafe());
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_ZeroMatches) {
  std::string highlighted_text;
  EXPECT_FALSE(FindAndHighlightWrapper("text", "query", &highlighted_text));
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_EmptyText) {
  std::string highlighted_text;
  EXPECT_FALSE(FindAndHighlightWrapper("", "query", &highlighted_text));
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_FullMatch) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlightWrapper("hello", "hello", &highlighted_text));
  EXPECT_EQ("<b>hello</b>", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_StartWith) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlightWrapper("hello, world", "hello",
                                     &highlighted_text));
  EXPECT_EQ("<b>hello</b>, world", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_EndWith) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlightWrapper("hello, world", "world",
                                     &highlighted_text));
  EXPECT_EQ("hello, <b>world</b>", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_InTheMiddle) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlightWrapper("yo hello, world", "hello",
                                     &highlighted_text));
  EXPECT_EQ("yo <b>hello</b>, world", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_MultipeMatches) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlightWrapper("yoyoyoyoy", "yoy", &highlighted_text));
  // Only the first match is highlighted.
  EXPECT_EQ("<b>yoy</b>oyoyoy", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_IgnoreCase) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlightWrapper("HeLLo", "hello", &highlighted_text));
  EXPECT_EQ("<b>HeLLo</b>", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_IgnoreCaseNonASCII) {
  std::string highlighted_text;

  // Case and accent ignorance in Greek. Find "socra" in "Socra'tes".
  EXPECT_TRUE(FindAndHighlightWrapper(
      "\xCE\xA3\xCF\x89\xCE\xBA\xCF\x81\xCE\xAC\xCF\x84\xCE\xB7\xCF\x82",
      "\xCF\x83\xCF\x89\xCE\xBA\xCF\x81\xCE\xB1", &highlighted_text));
  EXPECT_EQ(
      "<b>\xCE\xA3\xCF\x89\xCE\xBA\xCF\x81\xCE\xAC</b>\xCF\x84\xCE\xB7\xCF\x82",
      highlighted_text);

  // In Japanese characters.
  // Find Hiragana "pi" + "(small)ya" in Katakana "hi" + semi-voiced-mark + "ya"
  EXPECT_TRUE(FindAndHighlightWrapper(
      "\xE3\x81\xB2\xE3\x82\x9A\xE3\x82\x83\xE3\x83\xBC",
      "\xE3\x83\x94\xE3\x83\xA4",
      &highlighted_text));
  EXPECT_EQ(
      "<b>\xE3\x81\xB2\xE3\x82\x9A\xE3\x82\x83</b>\xE3\x83\xBC",
      highlighted_text);
}

TEST(SearchMetadataSimpleTest, MultiTextBySingleQuery) {
  base::i18n::FixedPatternStringSearchIgnoringCaseAndAccents query(
      base::UTF8ToUTF16("hello"));

  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlight("hello", &query, &highlighted_text));
  EXPECT_EQ("<b>hello</b>", highlighted_text);
  EXPECT_FALSE(FindAndHighlight("goodbye", &query, &highlighted_text));
  EXPECT_TRUE(FindAndHighlight("1hello2", &query, &highlighted_text));
  EXPECT_EQ("1<b>hello</b>2", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_MetaChars) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlightWrapper("<hello>", "hello", &highlighted_text));
  EXPECT_EQ("&lt;<b>hello</b>&gt;", highlighted_text);
}

TEST(SearchMetadataSimpleTest, FindAndHighlight_MoreMetaChars) {
  std::string highlighted_text;
  EXPECT_TRUE(FindAndHighlightWrapper("a&b&c&d", "b&c", &highlighted_text));
  EXPECT_EQ("a&amp;<b>b&amp;c</b>&amp;d", highlighted_text);
}

}  // namespace internal
}  // namespace drive
