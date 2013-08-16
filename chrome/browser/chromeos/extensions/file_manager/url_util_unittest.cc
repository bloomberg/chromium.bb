// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/extensions/file_manager/url_util.h"

#include "base/files/file_path.h"
#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "net/base/escape.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace file_manager {
namespace util {
namespace {

// Pretty print the JSON escaped in the query string.
std::string PrettyPrintEscapedJson(const std::string& query) {
  const std::string json = net::UnescapeURLComponent(
      query, net::UnescapeRule::SPACES | net::UnescapeRule::URL_SPECIAL_CHARS);
  scoped_ptr<base::Value> value(base::JSONReader::Read(json));
  std::string pretty_json;
  base::JSONWriter::WriteWithOptions(value.get(),
                                     base::JSONWriter::OPTIONS_PRETTY_PRINT,
                                     &pretty_json);
  return pretty_json;
}

TEST(FileManagerUrlUtilTest, GetFileManagerBaseUrl) {
  EXPECT_EQ("chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/",
            GetFileManagerBaseUrl().spec());
}

TEST(FileManagerUrlUtilTest, GetFileManagerMainPageUrl) {
  EXPECT_EQ("chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/main.html",
            GetFileManagerMainPageUrl().spec());
}

TEST(FileManagerUrlUtilTest, GetFileManagerMainPageUrlWithParams_NoFileTypes) {
  const GURL url = GetFileManagerMainPageUrlWithParams(
      ui::SelectFileDialog::SELECT_OPEN_FILE,
      base::UTF8ToUTF16("some title"),
      base::FilePath::FromUTF8Unsafe("foo.txt"),
      NULL,  // No file types
      0,  // Hence no file type index.
      FILE_PATH_LITERAL("txt"));
  EXPECT_EQ("chrome-extension", url.scheme());
  EXPECT_EQ("hhaomjibdihmijegdhdafkllkbggdgoj", url.host());
  EXPECT_EQ("/main.html", url.path());
  // Confirm that "%20" is used instead of "+" in the query.
  EXPECT_TRUE(url.query().find("+") == std::string::npos);
  EXPECT_TRUE(url.query().find("%20") != std::string::npos);
  // The escaped query is hard to read. Pretty print the escaped JSON.
  EXPECT_EQ("{\n"
            "   \"defaultExtension\": \"txt\",\n"
            "   \"defaultPath\": \"foo.txt\",\n"
            "   \"shouldReturnLocalPath\": true,\n"
            "   \"title\": \"some title\",\n"
            "   \"type\": \"open-file\"\n"
            "}\n",
            PrettyPrintEscapedJson(url.query()));
}

TEST(FileManagerUrlUtilTest,
     GetFileManagerMainPageUrlWithParams_WithFileTypes) {
  // Create a FileTypeInfo which looks like:
  // extensions: [["htm", "html"], ["txt"]]
  // descriptions: ["HTML", "TEXT"]
  ui::SelectFileDialog::FileTypeInfo file_types;
  file_types.extensions.push_back(std::vector<base::FilePath::StringType>());
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("htm"));
  file_types.extensions[0].push_back(FILE_PATH_LITERAL("html"));
  file_types.extensions.push_back(std::vector<base::FilePath::StringType>());
  file_types.extensions[1].push_back(FILE_PATH_LITERAL("txt"));
  file_types.extension_description_overrides.push_back(
      base::UTF8ToUTF16("HTML"));
  file_types.extension_description_overrides.push_back(
      base::UTF8ToUTF16("TEXT"));
  // "shouldReturnLocalPath" will be false if drive is supported.
  file_types.support_drive = true;

  const GURL url = GetFileManagerMainPageUrlWithParams(
      ui::SelectFileDialog::SELECT_OPEN_FILE,
      base::UTF8ToUTF16("some title"),
      base::FilePath::FromUTF8Unsafe("foo.txt"),
      &file_types,
      1,  // The file type index is 1-based.
      FILE_PATH_LITERAL("txt"));
  EXPECT_EQ("chrome-extension", url.scheme());
  EXPECT_EQ("hhaomjibdihmijegdhdafkllkbggdgoj", url.host());
  EXPECT_EQ("/main.html", url.path());
  // Confirm that "%20" is used instead of "+" in the query.
  EXPECT_TRUE(url.query().find("+") == std::string::npos);
  EXPECT_TRUE(url.query().find("%20") != std::string::npos);
  // The escaped query is hard to read. Pretty print the escaped JSON.
  EXPECT_EQ("{\n"
            "   \"defaultExtension\": \"txt\",\n"
            "   \"defaultPath\": \"foo.txt\",\n"
            "   \"includeAllFiles\": false,\n"
            "   \"shouldReturnLocalPath\": false,\n"
            "   \"title\": \"some title\",\n"
            "   \"type\": \"open-file\",\n"
            "   \"typeList\": [ {\n"
            "      \"description\": \"HTML\",\n"
            "      \"extensions\": [ \"htm\", \"html\" ],\n"
            "      \"selected\": true\n"
            "   }, {\n"
            "      \"description\": \"TEXT\",\n"
            "      \"extensions\": [ \"txt\" ],\n"
            "      \"selected\": false\n"
            "   } ]\n"
            "}\n",
            PrettyPrintEscapedJson(url.query()));
}

TEST(FileManagerUrlUtilTest, GetActionChoiceUrl_RegularMode) {
  EXPECT_EQ("chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/"
            "action_choice.html#/foo.txt",
            GetActionChoiceUrl(base::FilePath::FromUTF8Unsafe("foo.txt"),
                               false).spec());
}

TEST(FileManagerUrlUtilTest, GetActionChoiceUrl_AdvancedMode) {
  EXPECT_EQ("chrome-extension://hhaomjibdihmijegdhdafkllkbggdgoj/"
            "action_choice.html?advanced-mode#/foo.txt",
            GetActionChoiceUrl(base::FilePath::FromUTF8Unsafe("foo.txt"),
                               true).spec());
}

}  // namespace
}  // namespace util
}  // namespace file_manager
