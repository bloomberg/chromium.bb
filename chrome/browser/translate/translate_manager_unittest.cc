// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_manager.h"

#include "content/public/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

#ifdef FILE_MANAGER_EXTENSION
#include "chrome/browser/chromeos/extensions/file_manager/file_manager_util.h"
#include "extensions/common/constants.h"
#endif

typedef testing::Test TranslateManagerTest;

TEST_F(TranslateManagerTest, CheckTranslatableURL) {
  GURL empty_url = GURL(std::string());
  EXPECT_FALSE(TranslateManager::IsTranslatableURL(empty_url));

  std::string chrome = std::string(chrome::kChromeUIScheme) + "://flags";
  GURL chrome_url = GURL(chrome);
  EXPECT_FALSE(TranslateManager::IsTranslatableURL(chrome_url));

  std::string devtools = std::string(chrome::kChromeDevToolsScheme) + "://";
  GURL devtools_url = GURL(devtools);
  EXPECT_FALSE(TranslateManager::IsTranslatableURL(devtools_url));

#ifdef FILE_MANAGER_EXTENSION
  std::string filemanager =
      std::string(extensions::kExtensionScheme) +
      std::string("://") +
      std::string(kFileBrowserDomain);
  GURL filemanager_url = GURL(filemanager);
  EXPECT_FALSE(TranslateManager::IsTranslatableURL(filemanager_url));
#endif

  std::string ftp = std::string(chrome::kFtpScheme) + "://google.com/pub";
  GURL ftp_url = GURL(ftp);
  EXPECT_FALSE(TranslateManager::IsTranslatableURL(ftp_url));

  GURL right_url = GURL("http://www.tamurayukari.com/");
  EXPECT_TRUE(TranslateManager::IsTranslatableURL(right_url));
}
