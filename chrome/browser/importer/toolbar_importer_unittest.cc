// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "testing/gtest/include/gtest/gtest.h"

#include <string>
#include <vector>

#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/first_run/first_run.h"
#include "chrome/browser/first_run/first_run_internal.h"
#include "chrome/browser/importer/toolbar_importer.h"
#include "googleurl/src/gurl.h"
#include "third_party/libxml/chromium/libxml_utils.h"

// See http://crbug.com/11838
TEST(Toolbar5ImporterTest, BookmarkParse) {
static const string16 kTitle = ASCIIToUTF16("MyTitle");
static const char kUrl[] = "http://www.google.com/";
static const string16 kFolder = ASCIIToUTF16("Google");
static const string16 kFolder2 = ASCIIToUTF16("Homepage");
static const string16 kFolderArray[3] = {
  ASCIIToUTF16("Google"),
  ASCIIToUTF16("Search"),
  ASCIIToUTF16("Page")
};
static const string16 kOtherTitle = ASCIIToUTF16("MyOtherTitle");
static const char* kOtherUrl = "http://www.google.com/mail";
static const string16 kOtherFolder = ASCIIToUTF16("Mail");

static const string16 kBookmarkGroupTitle = ASCIIToUTF16("BookmarkGroupTitle");

// Since the following is very dense to read I enumerate the test cases here.
// 1. Correct bookmark structure with one label.
// 2. Correct bookmark structure with no labels.
// 3. Correct bookmark structure with two labels.
// 4. Correct bookmark structure with a folder->label translation by toolbar.
// 5. Correct bookmark structure with no favicon.
// 6. Two correct bookmarks.
// The following are error cases by removing sections from the xml:
// 7. Empty string passed as xml.
// 8. No <bookmarks> section in the xml.
// 9. No <bookmark> section below the <bookmarks> section.
// 10. No <title> in a <bookmark> section.
// 11. No <url> in a <bookmark> section.
// 12. No <timestamp> in a <bookmark> section.
// 13. No <labels> in a <bookmark> section.
static const char* kGoodBookmark =
    "<?xml version=\"1.0\" ?> <xml_api_reply version=\"1\"> <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const char* kGoodBookmarkNoLabel =
    "<?xml version=\"1.0\" ?> <xml_api_reply version=\"1\"> <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const char* kGoodBookmarkTwoLabels =
    "<?xml version=\"1.0\" ?> <xml_api_reply version=\"1\"> <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> <label>Homepage</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const char* kGoodBookmarkFolderLabel =
    "<?xml version=\"1.0\" ?> <xml_api_reply version=\"1\"> <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google:Search:Page</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const char* kGoodBookmarkNoFavicon =
    "<?xml version=\"1.0\" ?> <xml_api_reply version=\"1\"> <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const char* kGoodBookmark2Items =
    "<?xml version=\"1.0\" ?> <xml_api_reply version=\"1\"> <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark>"
    " <bookmark> "
    "<title>MyOtherTitle</title> "
    "<url>http://www.google.com/mail</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Mail</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name>"
    "<value>http://www.google.com/mail/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1253328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark>"
    "</bookmarks>";
static const char* kEmptyString = "";
static const char* kBadBookmarkNoBookmarks =
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const char* kBadBookmarkNoBookmark =
    "<?xml version=\"1.0\" ?> <xml_api_reply version=\"1\"> <bookmarks>"
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const char* kBadBookmarkNoTitle =
    "<?xml version=\"1.0\" ?> <xml_api_reply version=\"1\"> <bookmarks>"
    " <bookmark> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const char* kBadBookmarkNoUrl =
    "<?xml version=\"1.0\" ?> <xml_api_reply version=\"1\"> <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const char* kBadBookmarkNoTimestamp =
    "<?xml version=\"1.0\" ?> <xml_api_reply version=\"1\"> <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<labels> <label>Google</label> </labels> "
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";
static const char* kBadBookmarkNoLabels =
    "<?xml version=\"1.0\" ?> <xml_api_reply version=\"1\"> <bookmarks>"
    " <bookmark> "
    "<title>MyTitle</title> "
    "<url>http://www.google.com/</url> "
    "<timestamp>1153328691085181</timestamp> "
    "<id>N123nasdf239</id> <notebook_id>Bxxxxxxx</notebook_id> "
    "<section_id>Sxxxxxx</section_id> <has_highlight>0</has_highlight>"
    "<attributes> "
    "<attribute> "
    "<name>favicon_url</name> <value>http://www.google.com/favicon.ico</value> "
    "</attribute> "
    "<attribute> "
    "<name>favicon_timestamp</name> <value>1153328653</value> "
    "</attribute> "
    "<attribute> <name>notebook_name</name> <value>My notebook 0</value> "
    "</attribute> "
    "<attribute> <name>section_name</name> <value>My section 0 "
    "</value> </attribute> </attributes> "
    "</bookmark> </bookmarks>";

  XmlReader reader;
  std::string bookmark_xml;
  std::vector<ProfileWriter::BookmarkEntry> bookmarks;

  const GURL url(kUrl);
  const GURL other_url(kOtherUrl);

  // Test doesn't work if the importer thinks this is the first run of Chromium.
  // Mark this as a subsequent run of the browser.
  first_run::internal::first_run_ = first_run::internal::FIRST_RUN_FALSE;

  // Test case 1 is parsing a basic bookmark with a single label.
  bookmark_xml = kGoodBookmark;
  bookmarks.clear();
  XmlReader reader1;
  EXPECT_TRUE(reader1.Load(bookmark_xml));
  EXPECT_TRUE(Toolbar5Importer::ParseBookmarksFromReader(&reader1, &bookmarks,
      kBookmarkGroupTitle));

  ASSERT_EQ(1U, bookmarks.size());
  EXPECT_FALSE(bookmarks[0].in_toolbar);
  EXPECT_EQ(kTitle, bookmarks[0].title);
  EXPECT_EQ(url, bookmarks[0].url);
  ASSERT_EQ(2U, bookmarks[0].path.size());
  EXPECT_EQ(kFolder, bookmarks[0].path[1]);

  // Test case 2 is parsing a single bookmark with no label.
  bookmark_xml = kGoodBookmarkNoLabel;
  bookmarks.clear();
  XmlReader reader2;
  EXPECT_TRUE(reader2.Load(bookmark_xml));
  EXPECT_TRUE(Toolbar5Importer::ParseBookmarksFromReader(&reader2, &bookmarks,
      kBookmarkGroupTitle));

  ASSERT_EQ(1U, bookmarks.size());
  EXPECT_FALSE(bookmarks[0].in_toolbar);
  EXPECT_EQ(kTitle, bookmarks[0].title);
  EXPECT_EQ(url, bookmarks[0].url);
  EXPECT_EQ(1U, bookmarks[0].path.size());

  // Test case 3 is parsing a single bookmark with two labels.
  bookmark_xml = kGoodBookmarkTwoLabels;
  bookmarks.clear();
  XmlReader reader3;
  EXPECT_TRUE(reader3.Load(bookmark_xml));
  EXPECT_TRUE(Toolbar5Importer::ParseBookmarksFromReader(&reader3, &bookmarks,
      kBookmarkGroupTitle));

  ASSERT_EQ(2U, bookmarks.size());
  EXPECT_FALSE(bookmarks[0].in_toolbar);
  EXPECT_FALSE(bookmarks[1].in_toolbar);
  EXPECT_EQ(kTitle, bookmarks[0].title);
  EXPECT_EQ(kTitle, bookmarks[1].title);
  EXPECT_EQ(url, bookmarks[0].url);
  EXPECT_EQ(url, bookmarks[1].url);
  ASSERT_EQ(2U, bookmarks[0].path.size());
  EXPECT_EQ(kFolder, bookmarks[0].path[1]);
  ASSERT_EQ(2U, bookmarks[1].path.size());
  EXPECT_EQ(kFolder2, bookmarks[1].path[1]);

  // Test case 4 is parsing a single bookmark which has a label with a colon,
  // this test file name translation between Toolbar and Chrome.
  bookmark_xml = kGoodBookmarkFolderLabel;
  bookmarks.clear();
  XmlReader reader4;
  EXPECT_TRUE(reader4.Load(bookmark_xml));
  EXPECT_TRUE(Toolbar5Importer::ParseBookmarksFromReader(&reader4, &bookmarks,
      kBookmarkGroupTitle));

  ASSERT_EQ(1U, bookmarks.size());
  EXPECT_FALSE(bookmarks[0].in_toolbar);
  EXPECT_EQ(kTitle, bookmarks[0].title);
  EXPECT_EQ(url, bookmarks[0].url);
  ASSERT_EQ(4U, bookmarks[0].path.size());
  EXPECT_EQ(string16(kFolderArray[0]),
            bookmarks[0].path[1]);
  EXPECT_EQ(string16(kFolderArray[1]),
            bookmarks[0].path[2]);
  EXPECT_EQ(string16(kFolderArray[2]),
            bookmarks[0].path[3]);

  // Test case 5 is parsing a single bookmark without a favicon URL.
  bookmark_xml = kGoodBookmarkNoFavicon;
  bookmarks.clear();
  XmlReader reader5;
  EXPECT_TRUE(reader5.Load(bookmark_xml));
  EXPECT_TRUE(Toolbar5Importer::ParseBookmarksFromReader(&reader5, &bookmarks,
      kBookmarkGroupTitle));

  ASSERT_EQ(1U, bookmarks.size());
  EXPECT_FALSE(bookmarks[0].in_toolbar);
  EXPECT_EQ(kTitle, bookmarks[0].title);
  EXPECT_EQ(url, bookmarks[0].url);
  ASSERT_EQ(2U, bookmarks[0].path.size());
  EXPECT_EQ(kFolder, bookmarks[0].path[1]);

  // Test case 6 is parsing two bookmarks.
  bookmark_xml = kGoodBookmark2Items;
  bookmarks.clear();
  XmlReader reader6;
  EXPECT_TRUE(reader6.Load(bookmark_xml));
  EXPECT_TRUE(Toolbar5Importer::ParseBookmarksFromReader(&reader6, &bookmarks,
      kBookmarkGroupTitle));

  ASSERT_EQ(2U, bookmarks.size());
  EXPECT_FALSE(bookmarks[0].in_toolbar);
  EXPECT_FALSE(bookmarks[1].in_toolbar);
  EXPECT_EQ(kTitle, bookmarks[0].title);
  EXPECT_EQ(kOtherTitle, bookmarks[1].title);
  EXPECT_EQ(url, bookmarks[0].url);
  EXPECT_EQ(other_url, bookmarks[1].url);
  ASSERT_EQ(2U, bookmarks[0].path.size());
  EXPECT_EQ(kFolder, bookmarks[0].path[1]);
  ASSERT_EQ(2U, bookmarks[1].path.size());
  EXPECT_EQ(kOtherFolder, bookmarks[1].path[1]);

  // Test case 7 is parsing an empty string for bookmarks.
  bookmark_xml = kEmptyString;
  bookmarks.clear();
  XmlReader reader7;
  EXPECT_FALSE(reader7.Load(bookmark_xml));

  // Test case 8 is testing the error when no <bookmarks> section is present.
  bookmark_xml = kBadBookmarkNoBookmarks;
  bookmarks.clear();
  XmlReader reader8;
  EXPECT_TRUE(reader8.Load(bookmark_xml));
  EXPECT_FALSE(Toolbar5Importer::ParseBookmarksFromReader(&reader8,
      &bookmarks, kBookmarkGroupTitle));

  // Test case 9 tests when no <bookmark> section is present.
  bookmark_xml = kBadBookmarkNoBookmark;
  bookmarks.clear();
  XmlReader reader9;
  EXPECT_TRUE(reader9.Load(bookmark_xml));
  EXPECT_FALSE(Toolbar5Importer::ParseBookmarksFromReader(&reader9,
      &bookmarks, kBookmarkGroupTitle));


  // Test case 10 tests when a bookmark has no <title> section.
  bookmark_xml = kBadBookmarkNoTitle;
  bookmarks.clear();
  XmlReader reader10;
  EXPECT_TRUE(reader10.Load(bookmark_xml));
  EXPECT_FALSE(Toolbar5Importer::ParseBookmarksFromReader(&reader10,
      &bookmarks, kBookmarkGroupTitle));

  // Test case 11 tests when a bookmark has no <url> section.
  bookmark_xml = kBadBookmarkNoUrl;
  bookmarks.clear();
  XmlReader reader11;
  EXPECT_TRUE(reader11.Load(bookmark_xml));
  EXPECT_FALSE(Toolbar5Importer::ParseBookmarksFromReader(&reader11,
      &bookmarks, kBookmarkGroupTitle));

  // Test case 12 tests when a bookmark has no <timestamp> section.
  bookmark_xml = kBadBookmarkNoTimestamp;
  bookmarks.clear();
  XmlReader reader12;
  EXPECT_TRUE(reader12.Load(bookmark_xml));
  EXPECT_FALSE(Toolbar5Importer::ParseBookmarksFromReader(&reader12,
      &bookmarks, kBookmarkGroupTitle));

  // Test case 13 tests when a bookmark has no <labels> section.
  bookmark_xml = kBadBookmarkNoLabels;
  bookmarks.clear();
  XmlReader reader13;
  EXPECT_TRUE(reader13.Load(bookmark_xml));
  EXPECT_FALSE(Toolbar5Importer::ParseBookmarksFromReader(&reader13,
      &bookmarks, kBookmarkGroupTitle));
}
