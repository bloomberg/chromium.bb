// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/base64.h"
#include "base/base_paths_win.h"
#include "base/bind.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/stringprintf.h"
#include "base/test/scoped_path_override.h"
#include "chrome/browser/media_galleries/fileapi/itunes_finder.h"
#include "chrome/browser/media_galleries/fileapi/itunes_finder_win.h"
#include "chrome/common/chrome_paths.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace itunes {

namespace {

std::string EncodePath(const base::FilePath& path) {
  std::string input(reinterpret_cast<const char*>(path.value().data()),
                                                  path.value().size()*2);
  std::string result;
  base::Base64Encode(input, &result);
  return result;
}

void TouchFile(const base::FilePath& file) {
  ASSERT_EQ(1, file_util::WriteFile(file, " ", 1));
}

class ITunesFinderWinTest : public testing::Test {
 public:
  ITunesFinderWinTest()
    : ui_thread_(content::BrowserThread::UI, &loop_),
      file_thread_(content::BrowserThread::FILE, &loop_) {
  }

  virtual void SetUp() OVERRIDE {
    ASSERT_TRUE(app_data_dir_.CreateUniqueTempDir());
    ASSERT_TRUE(music_dir_.CreateUniqueTempDir());
    app_data_dir_override_.reset(
        new base::ScopedPathOverride(base::DIR_APP_DATA, app_data_dir()));
    music_dir_override_.reset(
        new base::ScopedPathOverride(chrome::DIR_USER_MUSIC, music_dir()));
  }

  const base::FilePath& app_data_dir() {
    return app_data_dir_.path();
  }

  const base::FilePath& music_dir() {
    return music_dir_.path();
  }

  void WritePrefFile(const std::string& data) {
    base::FilePath pref_dir =
        app_data_dir().AppendASCII("Apple Computer").AppendASCII("iTunes");
    ASSERT_TRUE(file_util::CreateDirectory(pref_dir));
    ASSERT_EQ(data.size(),
              file_util::WriteFile(pref_dir.AppendASCII("iTunesPrefs.xml"),
                                   data.data(), data.size()));
  }

  void TouchDefault() {
    base::FilePath default_dir = music_dir().AppendASCII("iTunes");
    ASSERT_TRUE(file_util::CreateDirectory(default_dir));
    TouchFile(default_dir.AppendASCII("iTunes Music Library.xml"));
  }

  std::string TestFindITunesLibrary() {
    std::string result;
    ITunesFinder::FindITunesLibrary(
        base::Bind(&ITunesFinderWinTest::FinderCallback,
                   base::Unretained(this), base::Unretained(&result)));
    base::RunLoop().RunUntilIdle();
    return result;
  }

 private:
  void FinderCallback(std::string* result_holder, const std::string& result) {
    *result_holder = result;
  }

  scoped_ptr<base::ScopedPathOverride> app_data_dir_override_;
  scoped_ptr<base::ScopedPathOverride> music_dir_override_;
  base::ScopedTempDir app_data_dir_;
  base::ScopedTempDir music_dir_;

  MessageLoop loop_;
  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread file_thread_;

  DISALLOW_COPY_AND_ASSIGN(ITunesFinderWinTest);
};

TEST_F(ITunesFinderWinTest, NotFound) {
  std::string result = TestFindITunesLibrary();
  EXPECT_TRUE(result.empty());
}

TEST_F(ITunesFinderWinTest, DefaultLocation) {
  TouchDefault();
  std::string result = TestFindITunesLibrary();
  EXPECT_FALSE(result.empty());
}

TEST_F(ITunesFinderWinTest, CustomLocation) {
  base::FilePath library_xml = music_dir().AppendASCII("library.xml");
  TouchFile(library_xml);
  std::string xml = base::StringPrintf(
      "<plist>"
      "  <dict>"
      "    <key>User Preferences</key>"
      "    <dict>"
      "      <key>iTunes Library XML Location:1</key>"
      "      <data>%s</data>"
      "    </dict>"
      "  </dict>"
      "</plist>", EncodePath(library_xml).c_str());
  WritePrefFile(xml);
  std::string result = TestFindITunesLibrary();
  EXPECT_FALSE(result.empty());
}

TEST_F(ITunesFinderWinTest, BadCustomLocation) {
  // Missing file.
  base::FilePath library_xml = music_dir().AppendASCII("library.xml");
  std::string xml = base::StringPrintf(
      "<plist>"
      "  <dict>"
      "    <key>User Preferences</key>"
      "    <dict>"
      "      <key>iTunes Library XML Location:1</key>"
      "      <data>%s</data>"
      "    </dict>"
      "  </dict>"
      "</plist>", EncodePath(library_xml).c_str());
  WritePrefFile(xml);
  std::string result = TestFindITunesLibrary();
  EXPECT_TRUE(result.empty());
  TouchFile(library_xml);

  // User Preferences dictionary at the wrong level.
  xml = base::StringPrintf(
      "<plist>"
      "    <key>User Preferences</key>"
      "    <dict>"
      "      <key>iTunes Library XML Location:1</key>"
      "      <data>%s</data>"
      "    </dict>"
      "</plist>", EncodePath(library_xml).c_str());
  WritePrefFile(xml);
  result = TestFindITunesLibrary();
  EXPECT_TRUE(result.empty());

  // Library location at the wrong scope.
  xml = base::StringPrintf(
      "<plist>"
      "  <dict>"
      "    <key>User Preferences</key>"
      "    <dict/>"
      "    <key>iTunes Library XML Location:1</key>"
      "    <data>%s</data>"
      "  </dict>"
      "</plist>", EncodePath(library_xml).c_str());
  WritePrefFile(xml);
  result = TestFindITunesLibrary();
  EXPECT_TRUE(result.empty());
}

}  // namespace

}  // namespace itunes
