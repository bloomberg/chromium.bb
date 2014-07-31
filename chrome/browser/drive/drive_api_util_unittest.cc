// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/drive/drive_api_util.h"

#include "base/files/scoped_temp_dir.h"
#include "base/md5.h"
#include "google_apis/drive/drive_api_parser.h"
#include "google_apis/drive/gdata_wapi_parser.h"
#include "google_apis/drive/test_util.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace drive {
namespace util {

TEST(DriveApiUtilTest, EscapeQueryStringValue) {
  EXPECT_EQ("abcde", EscapeQueryStringValue("abcde"));
  EXPECT_EQ("\\'", EscapeQueryStringValue("'"));
  EXPECT_EQ("\\'abcde\\'", EscapeQueryStringValue("'abcde'"));
  EXPECT_EQ("\\\\", EscapeQueryStringValue("\\"));
  EXPECT_EQ("\\\\\\'", EscapeQueryStringValue("\\'"));
}

TEST(DriveApiUtilTest, TranslateQuery) {
  EXPECT_EQ("", TranslateQuery(""));
  EXPECT_EQ("fullText contains 'dog'", TranslateQuery("dog"));
  EXPECT_EQ("fullText contains 'dog' and fullText contains 'cat'",
            TranslateQuery("dog cat"));
  EXPECT_EQ("not fullText contains 'cat'", TranslateQuery("-cat"));
  EXPECT_EQ("fullText contains 'dog cat'", TranslateQuery("\"dog cat\""));

  // Should handles full-width white space correctly.
  // Note: \xE3\x80\x80 (\u3000) is Ideographic Space (a.k.a. Japanese
  //   full-width whitespace).
  EXPECT_EQ("fullText contains 'dog' and fullText contains 'cat'",
            TranslateQuery("dog" "\xE3\x80\x80" "cat"));

  // If the quoted token is not closed (i.e. the last '"' is missing),
  // we handle the remaining string is one token, as a fallback.
  EXPECT_EQ("fullText contains 'dog cat'", TranslateQuery("\"dog cat"));

  // For quoted text with leading '-'.
  EXPECT_EQ("not fullText contains 'dog cat'", TranslateQuery("-\"dog cat\""));

  // Empty tokens should be simply ignored.
  EXPECT_EQ("", TranslateQuery("-"));
  EXPECT_EQ("", TranslateQuery("\"\""));
  EXPECT_EQ("", TranslateQuery("-\"\""));
  EXPECT_EQ("", TranslateQuery("\"\"\"\""));
  EXPECT_EQ("", TranslateQuery("\"\" \"\""));
  EXPECT_EQ("fullText contains 'dog'", TranslateQuery("\"\" dog \"\""));
}

TEST(FileSystemUtilTest, ExtractResourceIdFromUrl) {
  EXPECT_EQ("file:2_file_resource_id", ExtractResourceIdFromUrl(
      GURL("https://file1_link_self/file:2_file_resource_id")));
  // %3A should be unescaped.
  EXPECT_EQ("file:2_file_resource_id", ExtractResourceIdFromUrl(
      GURL("https://file1_link_self/file%3A2_file_resource_id")));

  // The resource ID cannot be extracted, hence empty.
  EXPECT_EQ("", ExtractResourceIdFromUrl(GURL("https://www.example.com/")));
}

TEST(FileSystemUtilTest, CanonicalizeResourceId) {
  std::string resource_id("1YsCnrMxxgp7LDdtlFDt-WdtEIth89vA9inrILtvK-Ug");

  // New style ID is unchanged.
  EXPECT_EQ(resource_id, CanonicalizeResourceId(resource_id));

  // Drop prefixes from old style IDs.
  EXPECT_EQ(resource_id, CanonicalizeResourceId("document:" + resource_id));
  EXPECT_EQ(resource_id, CanonicalizeResourceId("spreadsheet:" + resource_id));
  EXPECT_EQ(resource_id, CanonicalizeResourceId("presentation:" + resource_id));
  EXPECT_EQ(resource_id, CanonicalizeResourceId("drawing:" + resource_id));
  EXPECT_EQ(resource_id, CanonicalizeResourceId("table:" + resource_id));
  EXPECT_EQ(resource_id, CanonicalizeResourceId("externalapp:" + resource_id));
}

TEST(FileSystemUtilTest, ConvertFileResourceToResource_Parents) {
  google_apis::FileResource file_resource;

  std::vector<GURL> expected_links;
  expected_links.push_back(GURL("http://server/id1"));
  expected_links.push_back(GURL("http://server/id2"));
  expected_links.push_back(GURL("http://server/id3"));

  for (size_t i = 0; i < expected_links.size(); ++i) {
    google_apis::ParentReference parent;
    parent.set_parent_link(expected_links[i]);
    file_resource.mutable_parents()->push_back(parent);
  }

  scoped_ptr<google_apis::ResourceEntry> entry(
      ConvertFileResourceToResourceEntry(file_resource));
  std::vector<GURL> actual_links;
  for (size_t i = 0; i < entry->links().size(); ++i) {
    if (entry->links()[i]->type() == google_apis::Link::LINK_PARENT)
      actual_links.push_back(entry->links()[i]->href());
  }

  EXPECT_EQ(expected_links, actual_links);
}

TEST(FileSystemUtilTest, ConvertFileResourceToResourceEntryImageMediaMetadata) {
  google_apis::FileResource file_resource_all_fields;
  google_apis::FileResource file_resource_zero_fields;
  google_apis::FileResource file_resource_no_fields;
  // Set up FileResource instances;
  {
    {
      google_apis::ImageMediaMetadata* image_media_metadata =
        file_resource_all_fields.mutable_image_media_metadata();
      image_media_metadata->set_width(640);
      image_media_metadata->set_height(480);
      image_media_metadata->set_rotation(90);
    }
    {
      google_apis::ImageMediaMetadata* image_media_metadata =
        file_resource_zero_fields.mutable_image_media_metadata();
      image_media_metadata->set_width(0);
      image_media_metadata->set_height(0);
      image_media_metadata->set_rotation(0);
    }
  }

  // Verify the converted values.
  {
    scoped_ptr<google_apis::ResourceEntry> resource_entry(
        ConvertFileResourceToResourceEntry(file_resource_all_fields));

    EXPECT_EQ(640, resource_entry->image_width());
    EXPECT_EQ(480, resource_entry->image_height());
    EXPECT_EQ(90, resource_entry->image_rotation());
  }
  {
    scoped_ptr<google_apis::ResourceEntry> resource_entry(
        ConvertFileResourceToResourceEntry(file_resource_zero_fields));

    EXPECT_EQ(0, resource_entry->image_width());
    EXPECT_EQ(0, resource_entry->image_height());
    EXPECT_EQ(0, resource_entry->image_rotation());
  }
  {
    scoped_ptr<google_apis::ResourceEntry> resource_entry(
        ConvertFileResourceToResourceEntry(file_resource_no_fields));

    EXPECT_EQ(-1, resource_entry->image_width());
    EXPECT_EQ(-1, resource_entry->image_height());
    EXPECT_EQ(-1, resource_entry->image_rotation());
  }
}

TEST(DriveAPIUtilTest, GetMd5Digest) {
  base::ScopedTempDir temp_dir;
  ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

  base::FilePath path = temp_dir.path().AppendASCII("test.txt");
  const char kTestData[] = "abcdefghijklmnopqrstuvwxyz0123456789";
  ASSERT_TRUE(google_apis::test_util::WriteStringToFile(path, kTestData));

  EXPECT_EQ(base::MD5String(kTestData), GetMd5Digest(path));
}

TEST(DriveAPIUtilTest, HasHostedDocumentExtension) {
  EXPECT_TRUE(
      HasHostedDocumentExtension(base::FilePath::FromUTF8Unsafe("xx.gdoc")));
  EXPECT_TRUE(
      HasHostedDocumentExtension(base::FilePath::FromUTF8Unsafe("xx.gsheet")));
  EXPECT_TRUE(
      HasHostedDocumentExtension(base::FilePath::FromUTF8Unsafe("xx.gslides")));
  EXPECT_TRUE(
      HasHostedDocumentExtension(base::FilePath::FromUTF8Unsafe("xx.gdraw")));
  EXPECT_TRUE(
      HasHostedDocumentExtension(base::FilePath::FromUTF8Unsafe("xx.gtable")));
  EXPECT_TRUE(
      HasHostedDocumentExtension(base::FilePath::FromUTF8Unsafe("xx.gform")));
  EXPECT_TRUE(
      HasHostedDocumentExtension(base::FilePath::FromUTF8Unsafe("xx.glink")));

  EXPECT_FALSE(
      HasHostedDocumentExtension(base::FilePath::FromUTF8Unsafe("xx.gdocs")));
  EXPECT_FALSE(
      HasHostedDocumentExtension(base::FilePath::FromUTF8Unsafe("xx.docx")));
  EXPECT_FALSE(
      HasHostedDocumentExtension(base::FilePath::FromUTF8Unsafe("xx.jpg")));
}

}  // namespace util
}  // namespace drive
