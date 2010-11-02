// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Unit tests for ExtensionManifest.

#include <atlconv.h>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/values.h"
#include "ceee/ie/plugin/scripting/userscripts_librarian.h"
#include "ceee/testing/utils/test_utils.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const char kExtensionId[] = "abcdefghijklmnopqabcdefghijklmno";
const char kCssContent[] = "body:after {content: \"The End\";}";
const char kJsContent[] = "alert('red');";
const char kUrlPattern1[] = "http://madlymad.com/*";
const char kUrlPattern2[] = "https://superdave.com/*";
const char kUrl1[] = "http://madlymad.com/index.html";
const char kUrl2[] = "https://superdave.com/here.blue";
const char kUrl3[] = "http://not.here.com/there.where";
const wchar_t kJsPath1[] = L"script1.js";
const wchar_t kJsPath2[] = L"script2.js";
const wchar_t kCssFileName[] = L"CssFile.css";

TEST(UserScriptsLibrarianTest, Empty) {
  testing::LogDisabler no_dchecks;

  UserScriptsLibrarian librarian;
  librarian.AddUserScripts(UserScriptList());

  std::string css;
  EXPECT_HRESULT_SUCCEEDED(librarian.GetMatchingUserScriptsCssContent(
    GURL(), true, &css));
  EXPECT_TRUE(css.empty());

  UserScriptsLibrarian::JsFileList js;
  EXPECT_HRESULT_SUCCEEDED(librarian.GetMatchingUserScriptsJsContent(
      GURL(), UserScript::DOCUMENT_START, true, &js));
  EXPECT_TRUE(js.empty());
}

TEST(UserScriptsLibrarianTest, SingleScript) {
  testing::LogDisabler no_dchecks;

  URLPattern url_pattern(UserScript::kValidUserScriptSchemes);
  url_pattern.Parse(kUrlPattern1);

  UserScript user_script;
  user_script.add_url_pattern(url_pattern);
  user_script.set_extension_id(kExtensionId);
  user_script.set_run_location(UserScript::DOCUMENT_START);

  user_script.css_scripts().push_back(UserScript::File());
  UserScript::File& css_file = user_script.css_scripts().back();
  css_file.set_content(kCssContent);

  user_script.js_scripts().push_back(UserScript::File());
  UserScript::File& js_file = user_script.js_scripts().back();
  js_file.set_content(kJsContent);

  UserScriptList user_script_list;
  user_script_list.push_back(user_script);

  // Set up the librarian.
  UserScriptsLibrarian librarian;
  librarian.AddUserScripts(user_script_list);

  // Matching URL
  std::string css_content;
  EXPECT_HRESULT_SUCCEEDED(librarian.GetMatchingUserScriptsCssContent(
      GURL(kUrl1), false, &css_content));
  EXPECT_STREQ(kCssContent, css_content.c_str());

  // Non matching URL
  css_content.clear();
  EXPECT_HRESULT_SUCCEEDED(librarian.GetMatchingUserScriptsCssContent(
      GURL(kUrl2), false, &css_content));
  EXPECT_TRUE(css_content.empty());

  // Matching URL and run location.
  UserScriptsLibrarian::JsFileList js_content_list;
  EXPECT_HRESULT_SUCCEEDED(librarian.GetMatchingUserScriptsJsContent(
      GURL(kUrl1), UserScript::DOCUMENT_START, false, &js_content_list));
  ASSERT_EQ(1, js_content_list.size());
  EXPECT_STREQ(kJsContent, js_content_list[0].content.c_str());

  // Matching URL and non matching run location.
  js_content_list.clear();
  EXPECT_HRESULT_SUCCEEDED(librarian.GetMatchingUserScriptsJsContent(
      GURL(kUrl1), UserScript::DOCUMENT_END, false, &js_content_list));
  EXPECT_TRUE(js_content_list.empty());

  // Non matching URL and matching run location.
  js_content_list.clear();
  EXPECT_HRESULT_SUCCEEDED(librarian.GetMatchingUserScriptsJsContent(
      GURL(kUrl2), UserScript::DOCUMENT_START, false, &js_content_list));
  EXPECT_TRUE(js_content_list.empty());

  // Non matching URL and non matching run location.
  js_content_list.clear();
  EXPECT_HRESULT_SUCCEEDED(librarian.GetMatchingUserScriptsJsContent(
      GURL(kUrl2), UserScript::DOCUMENT_END, false, &js_content_list));
  EXPECT_TRUE(js_content_list.empty());
}

TEST(UserScriptsLibrarianTest, MultipleScript) {
  testing::LogDisabler no_dchecks;

  // Set up one UserScript object
  URLPattern url_pattern1(UserScript::kValidUserScriptSchemes);
  url_pattern1.Parse(kUrlPattern1);

  UserScript user_script1;
  user_script1.add_url_pattern(url_pattern1);
  user_script1.set_extension_id(kExtensionId);

  user_script1.css_scripts().push_back(UserScript::File());
  UserScript::File& css_file = user_script1.css_scripts().back();
  css_file.set_content(kCssContent);

  user_script1.js_scripts().push_back(UserScript::File());
  UserScript::File& js_file = user_script1.js_scripts().back();
  js_file.set_content(kJsContent);

  UserScriptList user_script_list1;
  user_script_list1.push_back(user_script1);

  // Set up a second UserScript object
  URLPattern url_pattern2(UserScript::kValidUserScriptSchemes);
  url_pattern2.Parse(kUrlPattern2);

  UserScript user_script2;
  user_script2.add_url_pattern(url_pattern2);
  user_script2.set_extension_id(kExtensionId);

  user_script2.js_scripts().push_back(UserScript::File());
  UserScript::File& js_file2 = user_script2.js_scripts().back();
  js_file2.set_content(kJsContent);

  user_script_list1.push_back(user_script2);

  // Set up the librarian.
  UserScriptsLibrarian librarian;
  librarian.AddUserScripts(user_script_list1);

  // Matching URL
  std::string css_content;
  EXPECT_HRESULT_SUCCEEDED(librarian.GetMatchingUserScriptsCssContent(
    GURL(kUrl1), false, &css_content));
  EXPECT_STREQ(css_content.c_str(), kCssContent);

  // Matching URL and non matching location
  UserScriptsLibrarian::JsFileList js_content_list;
  EXPECT_HRESULT_SUCCEEDED(librarian.GetMatchingUserScriptsJsContent(
      GURL(kUrl1), UserScript::DOCUMENT_START, false, &js_content_list));
  EXPECT_TRUE(js_content_list.empty());

  // Matching URL and location
  js_content_list.clear();
  EXPECT_HRESULT_SUCCEEDED(librarian.GetMatchingUserScriptsJsContent(
      GURL(kUrl1), UserScript::DOCUMENT_END, false, &js_content_list));
  ASSERT_EQ(1, js_content_list.size());
  EXPECT_STREQ(kJsContent, js_content_list[0].content.c_str());

  // Matching URL and location, shouldn't have extension init
  js_content_list.clear();
  EXPECT_HRESULT_SUCCEEDED(librarian.GetMatchingUserScriptsJsContent(
      GURL(kUrl2), UserScript::DOCUMENT_END, false, &js_content_list));
  ASSERT_EQ(1, js_content_list.size());
  EXPECT_STREQ(kJsContent, js_content_list[0].content.c_str());
}

TEST(UserScriptsLibrarianTest, SingleScriptFromFile) {
  testing::LogDisabler no_dchecks;

  URLPattern url_pattern(UserScript::kValidUserScriptSchemes);
  url_pattern.Parse(kUrlPattern1);

  UserScript user_script;
  user_script.add_url_pattern(url_pattern);
  user_script.set_extension_id(kExtensionId);
  user_script.set_run_location(UserScript::DOCUMENT_START);

  FilePath extension_folder;
  EXPECT_TRUE(file_util::GetTempDir(&extension_folder));
  FilePath css_path(extension_folder.Append(kCssFileName));

  user_script.css_scripts().push_back(UserScript::File(
      extension_folder, FilePath(kCssFileName), GURL()));

  FILE* temp_file = file_util::OpenFile(css_path, "w");
  EXPECT_TRUE(temp_file != NULL);

  fwrite(kCssContent, ::strlen(kCssContent), 1, temp_file);
  file_util::CloseFile(temp_file);
  temp_file = NULL;

  UserScriptList user_script_list;
  user_script_list.push_back(user_script);

  UserScriptsLibrarian librarian;
  librarian.AddUserScripts(user_script_list);

  EXPECT_TRUE(file_util::Delete(css_path, false));

  std::string css_content;
  EXPECT_HRESULT_SUCCEEDED(librarian.GetMatchingUserScriptsCssContent(
      GURL(kUrl1), false, &css_content));
  EXPECT_STREQ(kCssContent, css_content.c_str());

  UserScriptsLibrarian::JsFileList js_content_list;
  EXPECT_HRESULT_SUCCEEDED(librarian.GetMatchingUserScriptsJsContent(
      GURL(kUrl1), UserScript::DOCUMENT_START, false, &js_content_list));
  EXPECT_EQ(0, js_content_list.size());
}

TEST(UserScriptsLibrarianTest, MatchAllFramesFalse) {
  // all_frames is set to false. We should only get user scripts back
  // when we pass false for require_all_frames to GetMatching*.
  testing::LogDisabler no_dchecks;

  URLPattern url_pattern(UserScript::kValidUserScriptSchemes);
  url_pattern.Parse(kUrlPattern1);

  UserScript user_script;
  user_script.add_url_pattern(url_pattern);
  user_script.set_extension_id(kExtensionId);
  user_script.set_run_location(UserScript::DOCUMENT_START);

  user_script.css_scripts().push_back(UserScript::File());
  UserScript::File& css_file = user_script.css_scripts().back();
  css_file.set_content(kCssContent);

  user_script.js_scripts().push_back(UserScript::File());
  UserScript::File& js_file = user_script.js_scripts().back();
  js_file.set_content(kJsContent);

  UserScriptList user_script_list;
  user_script_list.push_back(user_script);

  UserScriptsLibrarian librarian;
  librarian.AddUserScripts(user_script_list);

  // Get CSS with require_all_frames set to false.
  std::string css_content;
  EXPECT_HRESULT_SUCCEEDED(librarian.GetMatchingUserScriptsCssContent(
      GURL(kUrl1), false, &css_content));
  EXPECT_STREQ(kCssContent, css_content.c_str());

  // Get CSS with require_all_frames set to true.
  css_content.clear();
  EXPECT_HRESULT_SUCCEEDED(librarian.GetMatchingUserScriptsCssContent(
      GURL(kUrl1), true, &css_content));
  EXPECT_TRUE(css_content.empty());

  // Get JS with require_all_frames set to false.
  UserScriptsLibrarian::JsFileList js_content_list;
  EXPECT_HRESULT_SUCCEEDED(librarian.GetMatchingUserScriptsJsContent(
      GURL(kUrl1), UserScript::DOCUMENT_START, false, &js_content_list));
  ASSERT_EQ(1, js_content_list.size());
  EXPECT_STREQ(kJsContent, js_content_list[0].content.c_str());

  // Get JS with require_all_frames set to true.
  js_content_list.clear();
  EXPECT_HRESULT_SUCCEEDED(librarian.GetMatchingUserScriptsJsContent(
      GURL(kUrl1), UserScript::DOCUMENT_START, true, &js_content_list));
  EXPECT_TRUE(js_content_list.empty());
}

TEST(UserScriptsLibrarianTest, MatchAllFramesTrue) {
  // all_frames is set to true. We should get user scripts back
  // when we pass any value for require_all_frames to GetMatching*.
  testing::LogDisabler no_dchecks;

  URLPattern url_pattern(UserScript::kValidUserScriptSchemes);
  url_pattern.Parse(kUrlPattern1);

  UserScript user_script;
  user_script.add_url_pattern(url_pattern);
  user_script.set_extension_id(kExtensionId);
  user_script.set_run_location(UserScript::DOCUMENT_START);
  user_script.set_match_all_frames(true);

  user_script.css_scripts().push_back(UserScript::File());
  UserScript::File& css_file = user_script.css_scripts().back();
  css_file.set_content(kCssContent);

  user_script.js_scripts().push_back(UserScript::File());
  UserScript::File& js_file = user_script.js_scripts().back();
  js_file.set_content(kJsContent);

  UserScriptList user_script_list;
  user_script_list.push_back(user_script);

  UserScriptsLibrarian librarian;
  librarian.AddUserScripts(user_script_list);

  // Get CSS with require_all_frames set to false.
  std::string css_content;
  EXPECT_HRESULT_SUCCEEDED(librarian.GetMatchingUserScriptsCssContent(
      GURL(kUrl1), false, &css_content));
  EXPECT_STREQ(kCssContent, css_content.c_str());

  // Get CSS with require_all_frames set to true.
  css_content.clear();
  EXPECT_HRESULT_SUCCEEDED(librarian.GetMatchingUserScriptsCssContent(
      GURL(kUrl1), true, &css_content));
  EXPECT_STREQ(kCssContent, css_content.c_str());

  // Get JS with require_all_frames set to false.
  UserScriptsLibrarian::JsFileList js_content_list;
  EXPECT_HRESULT_SUCCEEDED(librarian.GetMatchingUserScriptsJsContent(
      GURL(kUrl1), UserScript::DOCUMENT_START, false, &js_content_list));
  ASSERT_EQ(1, js_content_list.size());
  EXPECT_STREQ(kJsContent, js_content_list[0].content.c_str());

  // Get JS with require_all_frames set to true.
  js_content_list.clear();
  EXPECT_HRESULT_SUCCEEDED(librarian.GetMatchingUserScriptsJsContent(
      GURL(kUrl1), UserScript::DOCUMENT_START, true, &js_content_list));
  ASSERT_EQ(1, js_content_list.size());
  EXPECT_STREQ(kJsContent, js_content_list[0].content.c_str());
}

}  // namespace
