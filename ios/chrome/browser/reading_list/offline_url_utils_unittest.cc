// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ios/chrome/browser/reading_list/offline_url_utils.h"

#include <string>

#include "base/files/file_path.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

// Checks the root directory of offline pages.
TEST(OfflineURLUtilsTest, OfflineRootDirectoryPathTest) {
  base::FilePath profile_path("/profile_path");
  base::FilePath offline_directory =
      reading_list::OfflineRootDirectoryPath(profile_path);
  EXPECT_EQ("/profile_path/Offline", offline_directory.value());
}

// Checks the offline page directory is the MD5 of the URL
TEST(OfflineURLUtilsTest, OfflineURLDirectoryIDTest) {
  GURL url("http://www.google.com/test");
  // MD5 of "http://www.google.com/test"
  std::string md5 = "0090071ef710946a1263c276284bb3b8";
  std::string directory_id = reading_list::OfflineURLDirectoryID(url);
  EXPECT_EQ(md5, directory_id);
}

// Checks the offline page directory is
// |profile_path|/Offline/OfflineURLDirectoryID;
TEST(OfflineURLUtilsTest, OfflineURLDirectoryAbsolutePathTest) {
  base::FilePath profile_path("/profile_path");
  GURL url("http://www.google.com/test");
  base::FilePath offline_directory =
      reading_list::OfflineURLDirectoryAbsolutePath(profile_path, url);
  EXPECT_EQ("/profile_path/Offline/0090071ef710946a1263c276284bb3b8",
            offline_directory.value());
}

// Checks the offline page absolute path is
// |profile_path|/Offline/OfflineURLDirectoryID/page.html;
TEST(OfflineURLUtilsTest, OfflinePageAbsolutePathTest) {
  base::FilePath profile_path("/profile_path");
  GURL url("http://www.google.com/test");
  base::FilePath offline_page =
      reading_list::OfflinePageAbsolutePath(profile_path, url);
  EXPECT_EQ("/profile_path/Offline/0090071ef710946a1263c276284bb3b8/page.html",
            offline_page.value());
}

// Checks the offline page path is OfflineURLDirectoryID/page.html;
TEST(OfflineURLUtilsTest, OfflinePagePathTest) {
  GURL url("http://www.google.com/test");
  base::FilePath offline_page = reading_list::OfflinePagePath(url);
  EXPECT_EQ("0090071ef710946a1263c276284bb3b8/page.html", offline_page.value());
}

// Checks the distilled URL for the page is chrome://offline/MD5/page.html;
TEST(OfflineURLUtilsTest, DistilledURLForPathTest) {
  base::FilePath page_path("MD5/page.html");
  GURL distilled_url = reading_list::DistilledURLForPath(page_path);
  EXPECT_EQ("chrome://offline/MD5/page.html", distilled_url.spec());
}

// Checks the file path for chrome://offline/MD5/page.html is
// file://profile_path/Offline/MD5/page.html.
// Checks the resource root for chrome://offline/MD5/page.html is
// file://profile_path/Offline/MD5
TEST(OfflineURLUtilsTest, FileURLForDistilledURLTest) {
  base::FilePath profile_path("/profile_path");
  GURL file_url =
      reading_list::FileURLForDistilledURL(GURL(), profile_path, nullptr);
  EXPECT_FALSE(file_url.is_valid());

  GURL distilled_url("chrome://offline/MD5/page.html");
  file_url = reading_list::FileURLForDistilledURL(distilled_url, profile_path,
                                                  nullptr);
  EXPECT_TRUE(file_url.is_valid());
  EXPECT_TRUE(file_url.SchemeIsFile());
  EXPECT_EQ("/profile_path/Offline/MD5/page.html", file_url.path());

  GURL resource_url;
  file_url = reading_list::FileURLForDistilledURL(distilled_url, profile_path,
                                                  &resource_url);
  EXPECT_TRUE(resource_url.is_valid());
  EXPECT_TRUE(resource_url.SchemeIsFile());
  EXPECT_EQ("/profile_path/Offline/MD5/", resource_url.path());
}
