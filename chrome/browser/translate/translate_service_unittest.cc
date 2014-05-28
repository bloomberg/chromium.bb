// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/translate/translate_service.h"

#include "content/public/common/url_constants.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/file_manager/app_id.h"
#include "extensions/common/constants.h"
#endif

TEST(TranslateServiceTest, CheckTranslatableURL) {
  GURL empty_url = GURL(std::string());
  EXPECT_FALSE(TranslateService::IsTranslatableURL(empty_url));

  std::string chrome = std::string(content::kChromeUIScheme) + "://flags";
  GURL chrome_url = GURL(chrome);
  EXPECT_FALSE(TranslateService::IsTranslatableURL(chrome_url));

  std::string devtools = std::string(content::kChromeDevToolsScheme) + "://";
  GURL devtools_url = GURL(devtools);
  EXPECT_FALSE(TranslateService::IsTranslatableURL(devtools_url));

#if defined(OS_CHROMEOS)
  std::string filemanager = std::string(extensions::kExtensionScheme) +
                            std::string("://") +
                            std::string(file_manager::kFileManagerAppId);
  GURL filemanager_url = GURL(filemanager);
  EXPECT_FALSE(TranslateService::IsTranslatableURL(filemanager_url));
#endif

  std::string ftp = std::string(url::kFtpScheme) + "://google.com/pub";
  GURL ftp_url = GURL(ftp);
  EXPECT_FALSE(TranslateService::IsTranslatableURL(ftp_url));

  GURL right_url = GURL("http://www.tamurayukari.com/");
  EXPECT_TRUE(TranslateService::IsTranslatableURL(right_url));
}
