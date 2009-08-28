// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include "base/file_path.h"
#include "base/string_util.h"
#include "chrome/common/chrome_constants.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

#define FPL FILE_PATH_LITERAL

#if defined(OS_LINUX)
TEST(ShellIntegrationTest, GetDesktopShortcutFilename) {
  const struct {
    const FilePath::CharType* path;
    const char* url;
  } test_cases[] = {
    { FPL("http___foo_.desktop"), "http://foo" },
    { FPL("http___foo_bar_.desktop"), "http://foo/bar/" },
    { FPL("http___foo_bar_a=b&c=d.desktop"), "http://foo/bar?a=b&c=d" },

    // Now we're starting to be more evil...
    { FPL("http___foo_.desktop"), "http://foo/bar/baz/../../../../../" },
    { FPL("http___foo_.desktop"), "http://foo/bar/././../baz/././../" },
    { FPL("http___.._.desktop"), "http://../../../../" },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); i++) {
    EXPECT_EQ(WideToASCII(chrome::kBrowserProcessExecutableName) + "-" +
              test_cases[i].path,
              ShellIntegration::GetDesktopShortcutFilename(
                  GURL(test_cases[i].url)).value()) <<
        " while testing " << test_cases[i].url;
  }
}

TEST(ShellIntegrationTest, GetDesktopFileContents) {
  const struct {
    const char* url;
    const char* title;
    const char* template_contents;
    const char* expected_output;
  } test_cases[] = {
    // Dumb case.
    { "ignored", "ignored", "", "" },

    // Real-world case.
    { "http://gmail.com",
      "GMail",

      "[Desktop Entry]\n"
      "Version=1.0\n"
      "Encoding=UTF-8\n"
      "Name=Google Chrome\n"
      "Comment=The web browser from Google\n"
      "Exec=/opt/google/chrome/google-chrome %U\n"
      "Terminal=false\n"
      "Icon=/opt/google/chrome/product_logo_48.png\n"
      "Type=Application\n"
      "Categories=Application;Network;WebBrowser;\n"
      "MimeType=text/html;text/xml;application/xhtml_xml;\n",

      "[Desktop Entry]\n"
      "Version=1.0\n"
      "Encoding=UTF-8\n"
      "Name=GMail\n"
      "Exec=/opt/google/chrome/google-chrome \"--app=http://gmail.com/\"\n"
      "Terminal=false\n"
      "Icon=/opt/google/chrome/product_logo_48.png\n"
      "Type=Application\n"
      "Categories=Application;Network;WebBrowser;\n"
      "MimeType=text/html;text/xml;application/xhtml_xml;\n"
    },

    // Now we're starting to be more evil...
    { "http://evil.com/evil --join-the-b0tnet",
      "Ownz0red\nExec=rm -rf /",

      "Name=Google Chrome\n"
      "Exec=/opt/google/chrome/google-chrome %U\n",

      "Name=http://evil.com/evil%20--join-the-b0tnet\n"
      "Exec=/opt/google/chrome/google-chrome "
      "\"--app=http://evil.com/evil%%20--join-the-b0tnet\"\n"
    },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); i++) {
    EXPECT_EQ(test_cases[i].expected_output,
              ShellIntegration::GetDesktopFileContents(
                  test_cases[i].template_contents,
                  GURL(test_cases[i].url),
                  ASCIIToUTF16(test_cases[i].title)));
  }
}
#endif  // defined(OS_LINUX)
