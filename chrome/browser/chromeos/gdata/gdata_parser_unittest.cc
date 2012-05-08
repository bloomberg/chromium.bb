// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_file_value_serializer.h"
#include "base/path_service.h"
#include "base/string16.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "base/values.h"
#include "chrome/browser/chromeos/gdata/gdata_parser.h"
#include "chrome/common/chrome_paths.h"
#include "chrome/common/libxml_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Value;
using base::DictionaryValue;
using base::ListValue;

namespace gdata {

class GDataParserTest : public testing::Test {
 protected:
  static Value* LoadJSONFile(const std::string& filename) {
    FilePath path;
    std::string error;
    // Test files for this unit test are located in
    // src/chrome/test/data/chromeos/gdata/*
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

  static DocumentEntry* LoadDocumentEntryFromXml(const std::string& filename) {
    FilePath path;
    std::string error;
    PathService::Get(chrome::DIR_TEST_DATA, &path);
    path = path.AppendASCII("chromeos")
        .AppendASCII("gdata")
        .AppendASCII(filename.c_str());
    EXPECT_TRUE(file_util::PathExists(path)) <<
        "Couldn't find " << path.value();
    std::string contents;
    EXPECT_TRUE(file_util::ReadFileToString(path, &contents));
    XmlReader reader;
    if (!reader.Load(contents)) {
      NOTREACHED() << "Invalid xml:\n" << contents;
      return NULL;
    }
    scoped_ptr<DocumentEntry> entry;
    while (reader.Read()) {
      if (reader.NodeName() == "entry") {
        entry.reset(DocumentEntry::CreateFromXml(&reader));
        break;
      }
    }
    return entry.release();
  }
};

// Test document feed parsing.
TEST_F(GDataParserTest, DocumentFeedJsonParser) {
  std::string error;
  scoped_ptr<Value> document(LoadJSONFile("basic_feed.json"));
  ASSERT_TRUE(document.get());
  ASSERT_TRUE(document->GetType() == Value::TYPE_DICTIONARY);
  scoped_ptr<DocumentFeed> feed(DocumentFeed::ExtractAndParse(*document));
  ASSERT_TRUE(feed.get());

  base::Time update_time;
  ASSERT_TRUE(FeedEntry::GetTimeFromString("2011-12-14T01:03:21.151Z",
                                            &update_time));

  EXPECT_EQ(1, feed->start_index());
  EXPECT_EQ(1000, feed->items_per_page());
  EXPECT_EQ(update_time, feed->updated_time());

  // Check authors.
  ASSERT_EQ(1U, feed->authors().size());
  EXPECT_EQ(ASCIIToUTF16("tester"), feed->authors()[0]->name());
  EXPECT_EQ("tester@testing.com", feed->authors()[0]->email());

  // Check links.
  ASSERT_EQ(feed->links().size(), 6U);
  const Link* self_link = feed->GetLinkByType(Link::SELF);
  ASSERT_TRUE(self_link);
  EXPECT_EQ("https://self_link/", self_link->href().spec());
  EXPECT_EQ("application/atom+xml", self_link->mime_type());

  const Link* resumable_link =
      feed->GetLinkByType(Link::RESUMABLE_CREATE_MEDIA);
  ASSERT_TRUE(resumable_link);
  EXPECT_EQ("https://resumable_create_media_link/",
            resumable_link->href().spec());
  EXPECT_EQ("application/atom+xml", resumable_link->mime_type());

  // Check entries.
  ASSERT_EQ(3U, feed->entries().size());

  // Check a folder entry.
  const DocumentEntry* folder_entry = feed->entries()[0];
  ASSERT_TRUE(folder_entry);
  EXPECT_EQ(DocumentEntry::FOLDER, folder_entry->kind());
  EXPECT_EQ("\"HhMOFgcNHSt7ImBr\"", folder_entry->etag());
  EXPECT_EQ("folder:1_folder_resouce_id", folder_entry->resource_id());
  EXPECT_EQ("https://1_folder_id", folder_entry->id());
  EXPECT_EQ(ASCIIToUTF16("Entry 1 Title"), folder_entry->title());
  base::Time entry1_update_time;
  base::Time entry1_publish_time;
  ASSERT_TRUE(FeedEntry::GetTimeFromString("2011-04-01T18:34:08.234Z",
                                              &entry1_update_time));
  ASSERT_TRUE(FeedEntry::GetTimeFromString("2010-11-07T05:03:54.719Z",
                                              &entry1_publish_time));
  ASSERT_EQ(entry1_update_time, folder_entry->updated_time());
  ASSERT_EQ(entry1_publish_time, folder_entry->published_time());

  ASSERT_EQ(1U, folder_entry->authors().size());
  EXPECT_EQ(ASCIIToUTF16("entry_tester"), folder_entry->authors()[0]->name());
  EXPECT_EQ("entry_tester@testing.com", folder_entry->authors()[0]->email());
  EXPECT_EQ("https://1_folder_content_url/",
            folder_entry->content_url().spec());
  EXPECT_EQ("application/atom+xml;type=feed",
            folder_entry->content_mime_type());

  ASSERT_EQ(1U, folder_entry->feed_links().size());
  const FeedLink* feed_link = folder_entry->feed_links()[0];
  ASSERT_TRUE(feed_link);
  ASSERT_EQ(FeedLink::ACL, feed_link->type());

  const Link* entry1_alternate_link =
      folder_entry->GetLinkByType(Link::ALTERNATE);
  ASSERT_TRUE(entry1_alternate_link);
  EXPECT_EQ("https://1_folder_alternate_link/",
            entry1_alternate_link->href().spec());
  EXPECT_EQ("text/html", entry1_alternate_link->mime_type());

  const Link* entry1_edit_link = folder_entry->GetLinkByType(Link::EDIT);
  ASSERT_TRUE(entry1_edit_link);
  EXPECT_EQ("https://1_edit_link/", entry1_edit_link->href().spec());
  EXPECT_EQ("application/atom+xml", entry1_edit_link->mime_type());

  // Check a file entry.
  const DocumentEntry* file_entry = feed->entries()[1];
  ASSERT_TRUE(file_entry);
  EXPECT_EQ(DocumentEntry::FILE, file_entry->kind());
  EXPECT_EQ(ASCIIToUTF16("filename.m4a"), file_entry->filename());
  EXPECT_EQ(ASCIIToUTF16("sugg_file_name.m4a"),
            file_entry->suggested_filename());
  EXPECT_EQ("3b4382ebefec6e743578c76bbd0575ce", file_entry->file_md5());
  EXPECT_EQ(892721, file_entry->file_size());
  const Link* file_parent_link = file_entry->GetLinkByType(Link::PARENT);
  ASSERT_TRUE(file_parent_link);
  EXPECT_EQ("https://file_link_parent/", file_parent_link->href().spec());
  EXPECT_EQ("application/atom+xml", file_parent_link->mime_type());
  EXPECT_EQ(ASCIIToUTF16("Medical"), file_parent_link->title());

  // Check a file entry.
  const DocumentEntry* document_entry = feed->entries()[2];
  ASSERT_TRUE(document_entry);
  EXPECT_EQ(DocumentEntry::DOCUMENT, document_entry->kind());
}


// Test document feed parsing.
TEST_F(GDataParserTest, DocumentEntryXmlParser) {
  scoped_ptr<DocumentEntry> entry(LoadDocumentEntryFromXml("entry.xml"));
  ASSERT_TRUE(entry.get());

  EXPECT_EQ(DocumentEntry::FILE, entry->kind());
  EXPECT_EQ("\"HhMOFgcNHSt7ImBr\"", entry->etag());
  EXPECT_EQ("file:xml_file_resouce_id", entry->resource_id());
  EXPECT_EQ("https://xml_file_id", entry->id());
  EXPECT_EQ(ASCIIToUTF16("Xml Entry File Title.tar"), entry->title());
  base::Time entry1_update_time;
  base::Time entry1_publish_time;
  ASSERT_TRUE(FeedEntry::GetTimeFromString("2011-04-01T18:34:08.234Z",
                                              &entry1_update_time));
  ASSERT_TRUE(FeedEntry::GetTimeFromString("2010-11-07T05:03:54.719Z",
                                              &entry1_publish_time));
  ASSERT_EQ(entry1_update_time, entry->updated_time());
  ASSERT_EQ(entry1_publish_time, entry->published_time());

  ASSERT_EQ(1U, entry->authors().size());
  EXPECT_EQ(ASCIIToUTF16("entry_tester"), entry->authors()[0]->name());
  EXPECT_EQ("entry_tester@testing.com", entry->authors()[0]->email());
  EXPECT_EQ("https://1_xml_file_entry_content_url/",
            entry->content_url().spec());
  EXPECT_EQ("application/x-tar",
            entry->content_mime_type());

  // Check feed links.
  ASSERT_EQ(2U, entry->feed_links().size());
  const FeedLink* feed_link_1 = entry->feed_links()[0];
  ASSERT_TRUE(feed_link_1);
  ASSERT_EQ(FeedLink::ACL, feed_link_1->type());
  const FeedLink* feed_link_2 = entry->feed_links()[1];
  ASSERT_TRUE(feed_link_2);
  ASSERT_EQ(FeedLink::REVISIONS, feed_link_2->type());

  // Check links.
  ASSERT_EQ(7U, entry->links().size());
  const Link* entry1_alternate_link = entry->GetLinkByType(Link::ALTERNATE);
  ASSERT_TRUE(entry1_alternate_link);
  EXPECT_EQ("https://xml_file_entry_id_alternate_link/",
            entry1_alternate_link->href().spec());
  EXPECT_EQ("text/html", entry1_alternate_link->mime_type());

  const Link* entry1_edit_link = entry->GetLinkByType(Link::EDIT_MEDIA);
  ASSERT_TRUE(entry1_edit_link);
  EXPECT_EQ("https://xml_file_entry_id_edit_media_link/",
            entry1_edit_link->href().spec());
  EXPECT_EQ("application/x-tar", entry1_edit_link->mime_type());

  const Link* entry1_self_link = entry->GetLinkByType(Link::SELF);
  ASSERT_TRUE(entry1_self_link);
  EXPECT_EQ("https://xml_file_entry_id_self_link/",
            entry1_self_link->href().spec());
  EXPECT_EQ("application/atom+xml", entry1_self_link->mime_type());

  // Check a file properties.
  EXPECT_EQ(DocumentEntry::FILE, entry->kind());
  EXPECT_EQ(ASCIIToUTF16("Xml Entry File Name.tar"), entry->filename());
  EXPECT_EQ(ASCIIToUTF16("Xml Entry Suggested File Name.tar"),
            entry->suggested_filename());
  EXPECT_EQ("e48f4d5c46a778de263e0e3f4b3d2a7d", entry->file_md5());
  EXPECT_EQ(26562560, entry->file_size());
}

TEST_F(GDataParserTest, AccountMetadataFeedParser) {
  scoped_ptr<Value> document(LoadJSONFile("account_metadata.json"));
  ASSERT_TRUE(document.get());
  ASSERT_TRUE(document->GetType() == Value::TYPE_DICTIONARY);
  DictionaryValue* entry_value;
  ASSERT_TRUE(reinterpret_cast<DictionaryValue*>(document.get())->GetDictionary(
      std::string("entry"), &entry_value));
  ASSERT_TRUE(entry_value);

  scoped_ptr<AccountMetadataFeed> feed(
      AccountMetadataFeed::CreateFrom(*document));
  ASSERT_TRUE(feed.get());
  EXPECT_EQ(GG_LONGLONG(6789012345), feed->quota_bytes_used());
  EXPECT_EQ(GG_LONGLONG(9876543210), feed->quota_bytes_total());
  EXPECT_EQ(654321, feed->largest_changestamp());
  ASSERT_EQ(2U, feed->installed_apps()->size());
  const InstalledApp* first_app = feed->installed_apps()[0];
  const InstalledApp* second_app = feed->installed_apps()[1];

  EXPECT_EQ("Drive App 1", UTF16ToUTF8(first_app->app_name()));
  EXPECT_EQ("Drive App Object 1", UTF16ToUTF8(first_app->object_type()));
  EXPECT_TRUE(first_app->supports_create());
  EXPECT_EQ("https://chrome.google.com/webstore/detail/11111111",
            first_app->GetProductUrl().spec());
  ASSERT_EQ(2U, first_app->primary_mimetypes()->size());
  EXPECT_EQ("application/test_type_1",
            *first_app->primary_mimetypes()->at(0));
  EXPECT_EQ("application/vnd.google-apps.drive-sdk.11111111",
            *first_app->primary_mimetypes()->at(1));
  ASSERT_EQ(1U, first_app->secondary_mimetypes()->size());
  EXPECT_EQ("image/jpeg", *first_app->secondary_mimetypes()->at(0));
  ASSERT_EQ(2U, first_app->primary_extensions()->size());
  EXPECT_EQ("ext_1", *first_app->primary_extensions()->at(0));
  EXPECT_EQ("ext_2", *first_app->primary_extensions()->at(1));
  ASSERT_EQ(1U, first_app->secondary_extensions()->size());
  EXPECT_EQ("ext_3", *first_app->secondary_extensions()->at(0));

  EXPECT_EQ("Drive App 2", UTF16ToUTF8(second_app->app_name()));
  EXPECT_EQ("Drive App Object 2", UTF16ToUTF8(second_app->object_type()));
  EXPECT_EQ("https://chrome.google.com/webstore/detail/22222222",
            second_app->GetProductUrl().spec());
  EXPECT_FALSE(second_app->supports_create());
  EXPECT_EQ(2U, second_app->primary_mimetypes()->size());
  EXPECT_EQ(0U, second_app->secondary_mimetypes()->size());
  EXPECT_EQ(1U, second_app->primary_extensions()->size());
  EXPECT_EQ(0U, second_app->secondary_extensions()->size());
}

// Test file extension checking in DocumentEntry::HasDocumentExtension().
TEST_F(GDataParserTest, DocumentEntryHasDocumentExtension) {
  EXPECT_TRUE(DocumentEntry::HasHostedDocumentExtension(
      FilePath(FILE_PATH_LITERAL("Test.gdoc"))));
  EXPECT_TRUE(DocumentEntry::HasHostedDocumentExtension(
      FilePath(FILE_PATH_LITERAL("Test.gsheet"))));
  EXPECT_TRUE(DocumentEntry::HasHostedDocumentExtension(
      FilePath(FILE_PATH_LITERAL("Test.gslides"))));
  EXPECT_TRUE(DocumentEntry::HasHostedDocumentExtension(
      FilePath(FILE_PATH_LITERAL("Test.gdraw"))));
  EXPECT_TRUE(DocumentEntry::HasHostedDocumentExtension(
      FilePath(FILE_PATH_LITERAL("Test.gtable"))));
  EXPECT_FALSE(DocumentEntry::HasHostedDocumentExtension(
      FilePath(FILE_PATH_LITERAL("Test.tar.gz"))));
  EXPECT_FALSE(DocumentEntry::HasHostedDocumentExtension(
      FilePath(FILE_PATH_LITERAL("Test.txt"))));
  EXPECT_FALSE(DocumentEntry::HasHostedDocumentExtension(
      FilePath(FILE_PATH_LITERAL("Test"))));
  EXPECT_FALSE(DocumentEntry::HasHostedDocumentExtension(
      FilePath(FILE_PATH_LITERAL(""))));
}

}  // namespace gdata
