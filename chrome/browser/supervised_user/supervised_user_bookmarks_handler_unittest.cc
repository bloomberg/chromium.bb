// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/supervised_user/supervised_user_bookmarks_handler.h"
#include "testing/gtest/include/gtest/gtest.h"

using Folder = SupervisedUserBookmarksHandler::Folder;
using Link = SupervisedUserBookmarksHandler::Link;

namespace {

typedef std::pair<std::string, std::string> Setting;

// Settings representing the following tree:
// |--Folder1
//    |--SubFolder
//       |--Empty SubSubFolder
//       |--Google(google.com)
//       |--(www.theoatmeal.com)
//    |--Test(www.test.de)
// |--Empty Folder
// |--XKCD(m.xkcd.com)

// Folder setting format: "<ID>", "<ParentID>:<Name>"
const Setting FOLDER_SETTINGS[] = {
  Setting("5", "0:Folder1"),
  Setting("9", "0:Empty Folder"),
  Setting("3", "5:SubFolder"),
  Setting("4", "3:Empty SubSubFolder"),
};

const Setting FOLDER_SETTINGS_INVALID_PARENT[] = {
    Setting("5", "0:Folder1"),
    Setting("9", "7:Empty Folder"),  // Invalid parent id.
    Setting("3", "5:SubFolder"),
    Setting("4", "3:Empty SubSubFolder"),
};

const Setting FOLDER_SETTINGS_CIRCLE[] = {
  Setting("5", "3:Folder1"),
  Setting("9", "5:Empty Folder"),
  Setting("3", "9:SubFolder"),
};

// Link setting format: "<ID>:<URL>", "<ParentID>:<Name>"
const Setting LINK_SETTINGS[] = {
  Setting("4:www.theoatmeal.com", "3:"),
  Setting("1:m.xkcd.com", "0:XKCD"),
  Setting("2:www.test.de", "5:Test"),
  Setting("3:google.com", "3:Google"),
};

const Setting LINK_SETTINGS_INVALID_PARENT[] = {
  Setting("4:www.theoatmeal.com", "3:"),
  Setting("1:m.xkcd.com", "7:XKCD"),  // Invalid parent id.
  Setting("2:www.test.de", "8:Test"),  // Invalid parent id.
};

// Parsed data is sorted by ID (even though the parsed links don't actually
// contain their IDs!)

const Folder PARSED_FOLDERS[] = {
  Folder(3, "SubFolder", 5),  // Note: Forward reference.
  Folder(4, "Empty SubSubFolder", 3),
  Folder(5, "Folder1", 0),
  Folder(9, "Empty Folder", 0),
};

const Link PARSED_LINKS[] = {
  Link("m.xkcd.com", "XKCD", 0),
  Link("www.test.de", "Test", 5),
  Link("google.com", "Google", 3),
  Link("www.theoatmeal.com", std::string(), 3),
};

const char BOOKMARKS_TREE_JSON[] =
    "["
    "  {"
    "    \"id\":5,"
    "    \"name\":\"Folder1\","
    "    \"children\":["
    "      {"
    "        \"id\":3,"
    "        \"name\":\"SubFolder\","
    "        \"children\":["
    "          {"
    "            \"id\":4,"
    "            \"name\":\"Empty SubSubFolder\","
    "            \"children\":[]"
    "          },"
    "          {"
    "            \"name\":\"Google\","
    "            \"url\":\"http://google.com/\""
    "          },"
    "          {"
    "            \"name\":\"\","
    "            \"url\":\"http://www.theoatmeal.com/\""
    "          }"
    "        ]"
    "      },"
    "      {"
    "        \"name\":\"Test\","
    "        \"url\":\"http://www.test.de/\""
    "      }"
    "    ]"
    "  },"
    "  {"
    "    \"id\":9,"
    "    \"name\":\"Empty Folder\","
    "    \"children\":[]"
    "  },"
    "  {"
    "    \"name\":\"XKCD\","
    "    \"url\":\"http://m.xkcd.com/\""
    "  }"
    "]";

const char BOOKMARKS_TREE_INVALID_PARENTS_JSON[] =
    "["
    "  {"
    "    \"id\":5,"
    "    \"name\":\"Folder1\","
    "    \"children\":["
    "      {"
    "        \"id\":3,"
    "        \"name\":\"SubFolder\","
    "        \"children\":["
    "          {"
    "            \"id\":4,"
    "            \"name\":\"Empty SubSubFolder\","
    "            \"children\":[]"
    "          },"
    "          {"
    "            \"name\":\"\","
    "            \"url\":\"http://www.theoatmeal.com/\""
    "          }"
    "        ]"
    "      }"
    "    ]"
    "  }"
    "]";

// Builds the base::Values tree from a json string above.
scoped_ptr<base::ListValue> CreateTree(const char* json) {
  base::Value* value = base::JSONReader::Read(json);
  EXPECT_NE(value, nullptr);
  base::ListValue* list;
  EXPECT_TRUE(value->GetAsList(&list));
  return make_scoped_ptr(list);
}

scoped_ptr<base::ListValue> CreateBookmarksTree() {
  return CreateTree(BOOKMARKS_TREE_JSON);
}

scoped_ptr<base::ListValue> CreateBookmarksTreeWithInvalidParents() {
  return CreateTree(BOOKMARKS_TREE_INVALID_PARENTS_JSON);
}

}  // namespace

static bool operator==(const Folder& f1, const Folder& f2) {
  return f1.id == f2.id && f1.name == f2.name && f1.parent_id == f2.parent_id;
}

static bool operator==(const Link& l1, const Link& l2) {
  return l1.url == l2.url && l1.name == l2.name && l1.parent_id == l2.parent_id;
}

std::ostream& operator<<(std::ostream& str, const Folder& folder) {
  str << folder.id << " " << folder.name << " " << folder.parent_id;
  return str;
}

std::ostream& operator<<(std::ostream& str, const Link& link) {
  str << link.url << " " << link.name << " " << link.parent_id;
  return str;
}

class SupervisedUserBookmarksHandlerTest : public ::testing::Test {
 protected:
  static base::DictionaryValue* CreateSettings(base::DictionaryValue* links,
                                               base::DictionaryValue* folders) {
    base::DictionaryValue* settings = new base::DictionaryValue;
    settings->SetStringWithoutPathExpansion("some_setting", "bleh");
    settings->SetWithoutPathExpansion("SupervisedBookmarkLink", links);
    settings->SetStringWithoutPathExpansion("some_other_setting", "foo");
    settings->SetWithoutPathExpansion("SupervisedBookmarkFolder", folders);
    settings->SetStringWithoutPathExpansion("another_one", "blurb");
    return settings;
  }

  static base::DictionaryValue* CreateDictionary(const Setting* begin,
                                                 const Setting* end) {
    base::DictionaryValue* dict = new base::DictionaryValue;
    for (const Setting* setting = begin; setting != end; ++setting)
      dict->SetStringWithoutPathExpansion(setting->first, setting->second);
    return dict;
  }

  static base::DictionaryValue* CreateLinkDictionary() {
    return CreateDictionary(LINK_SETTINGS,
                            LINK_SETTINGS + arraysize(LINK_SETTINGS));
  }

  static base::DictionaryValue* CreateLinkDictionaryWithInvalidParents() {
    return CreateDictionary(
        LINK_SETTINGS_INVALID_PARENT,
        LINK_SETTINGS_INVALID_PARENT + arraysize(LINK_SETTINGS_INVALID_PARENT));
  }

  static base::DictionaryValue* CreateFolderDictionary() {
    return CreateDictionary(FOLDER_SETTINGS,
                            FOLDER_SETTINGS + arraysize(FOLDER_SETTINGS));
  }

  static base::DictionaryValue* CreateFolderDictionaryWithInvalidParents() {
    return CreateDictionary(
        FOLDER_SETTINGS_INVALID_PARENT,
        FOLDER_SETTINGS_INVALID_PARENT +
            arraysize(FOLDER_SETTINGS_INVALID_PARENT));
  }

  static base::DictionaryValue* CreateFolderDictionaryWithCircle() {
    return CreateDictionary(
        FOLDER_SETTINGS_CIRCLE,
        FOLDER_SETTINGS_CIRCLE + arraysize(FOLDER_SETTINGS_CIRCLE));
  }

  void ParseFolders(const base::DictionaryValue& folders) {
    deserializer_.ParseFolders(folders);
  }

  void ParseLinks(const base::DictionaryValue& links) {
    deserializer_.ParseLinks(links);
  }

  const std::vector<Folder>& GetFolders() const {
    return deserializer_.folders_for_testing();
  }

  const std::vector<Link>& GetLinks() const {
    return deserializer_.links_for_testing();
  }

 private:
  SupervisedUserBookmarksHandler deserializer_;
};

TEST_F(SupervisedUserBookmarksHandlerTest, ParseSettings) {
  scoped_ptr<base::DictionaryValue> link_dictionary(CreateLinkDictionary());
  scoped_ptr<base::DictionaryValue> folder_dictionary(CreateFolderDictionary());

  ParseLinks(*link_dictionary.get());
  ParseFolders(*folder_dictionary.get());

  const std::vector<Link>& links = GetLinks();
  EXPECT_EQ(arraysize(PARSED_LINKS), links.size());
  for (size_t i = 0; i < links.size(); ++i)
    EXPECT_EQ(PARSED_LINKS[i], links[i]);

  const std::vector<Folder>& folders = GetFolders();
  EXPECT_EQ(arraysize(PARSED_FOLDERS), folders.size());
  for (size_t i = 0; i < folders.size(); ++i)
    EXPECT_EQ(PARSED_FOLDERS[i], folders[i]);
}

TEST_F(SupervisedUserBookmarksHandlerTest, BuildBookmarksTree) {
  // Make some fake settings.
  scoped_ptr<base::DictionaryValue> settings(
      CreateSettings(CreateLinkDictionary(), CreateFolderDictionary()));
  // Parse the settings into a bookmarks tree.
  scoped_ptr<base::ListValue> bookmarks(
      SupervisedUserBookmarksHandler::BuildBookmarksTree(*settings.get()));

  // Check that the parsed tree matches the expected tree constructed directly
  // from the hardcoded json above.
  scoped_ptr<base::ListValue> expected_bookmarks(CreateBookmarksTree());
  EXPECT_TRUE(bookmarks->Equals(expected_bookmarks.get()));
}

TEST_F(SupervisedUserBookmarksHandlerTest,
       BuildBookmarksTreeWithInvalidParents) {
  // Make some fake settings, including some entries with invalid parent
  // references.
  scoped_ptr<base::DictionaryValue> settings(
      CreateSettings(CreateLinkDictionaryWithInvalidParents(),
                     CreateFolderDictionaryWithInvalidParents()));
  // Parse the settings into a bookmarks tree.
  scoped_ptr<base::ListValue> bookmarks(
      SupervisedUserBookmarksHandler::BuildBookmarksTree(*settings.get()));

  // Check that the parsed tree matches the expected tree constructed directly
  // from the hardcoded json above (which does not contain the entries with
  // invalid parents!).
  scoped_ptr<base::ListValue> expected_bookmarks(
      CreateBookmarksTreeWithInvalidParents());
  EXPECT_TRUE(bookmarks->Equals(expected_bookmarks.get()));
}

TEST_F(SupervisedUserBookmarksHandlerTest, Circle) {
  // Make some fake settings which include a circular reference in the folders.
  scoped_ptr<base::DictionaryValue> settings(
      CreateSettings(CreateLinkDictionary(),
                     CreateFolderDictionaryWithCircle()));
  scoped_ptr<base::ListValue> bookmarks(
      SupervisedUserBookmarksHandler::BuildBookmarksTree(*settings.get()));
  // Don't care what exactly the result looks like, just that we don't run into
  // an endless loop.
}
