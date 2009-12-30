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
    const char* icon_name;
    const char* template_contents;
    const char* expected_output;
  } test_cases[] = {
    // Dumb case.
    { "ignored", "ignored", "ignored", "", "#!/usr/bin/env xdg-open\n" },

    // Real-world case.
    { "http://gmail.com",
      "GMail",
      "chrome-http__gmail.com",

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

      "#!/usr/bin/env xdg-open\n"
      "[Desktop Entry]\n"
      "Version=1.0\n"
      "Encoding=UTF-8\n"
      "Name=GMail\n"
      "Exec=/opt/google/chrome/google-chrome --app=\"http://gmail.com/\"\n"
      "Terminal=false\n"
      "Icon=chrome-http__gmail.com\n"
      "Type=Application\n"
      "Categories=Application;Network;WebBrowser;\n"
      "MimeType=text/html;text/xml;application/xhtml_xml;\n"
    },

    // Make sure we don't insert duplicate shebangs.
    { "http://gmail.com",
      "GMail",
      "chrome-http__gmail.com",

      "#!/some/shebang\n"
      "Name=Google Chrome\n"
      "Exec=/opt/google/chrome/google-chrome %U\n",

      "#!/usr/bin/env xdg-open\n"
      "Name=GMail\n"
      "Exec=/opt/google/chrome/google-chrome --app=\"http://gmail.com/\"\n"
    },

    // Make sure i18n-ed comments are removed.
    { "http://gmail.com",
      "GMail",
      "chrome-http__gmail.com",

      "Name=Google Chrome\n"
      "Exec=/opt/google/chrome/google-chrome %U\n"
      "Comment[pl]=Jakis komentarz.\n",

      "#!/usr/bin/env xdg-open\n"
      "Name=GMail\n"
      "Exec=/opt/google/chrome/google-chrome --app=\"http://gmail.com/\"\n"
    },

    // Make sure that empty icons are replaced by the chrome icon.
    { "http://gmail.com",
      "GMail",
      "",

      "Name=Google Chrome\n"
      "Exec=/opt/google/chrome/google-chrome %U\n"
      "Comment[pl]=Jakis komentarz.\n"
      "Icon=/opt/google/chrome/product_logo_48.png\n",

      "#!/usr/bin/env xdg-open\n"
      "Name=GMail\n"
      "Exec=/opt/google/chrome/google-chrome --app=\"http://gmail.com/\"\n"
      "Icon=/opt/google/chrome/product_logo_48.png\n"
    },

    // Now we're starting to be more evil...
    { "http://evil.com/evil --join-the-b0tnet",
      "Ownz0red\nExec=rm -rf /",
      "chrome-http__evil.com_evil",

      "Name=Google Chrome\n"
      "Exec=/opt/google/chrome/google-chrome %U\n",

      "#!/usr/bin/env xdg-open\n"
      "Name=http://evil.com/evil%20--join-the-b0tnet\n"
      "Exec=/opt/google/chrome/google-chrome "
      "--app=\"http://evil.com/evil%20--join-the-b0tnet\"\n"
    },
    { "http://evil.com/evil; rm -rf /; \"; rm -rf $HOME >ownz0red",
      "Innocent Title",
      "chrome-http__evil.com_evil",

      "Name=Google Chrome\n"
      "Exec=/opt/google/chrome/google-chrome %U\n",

      "#!/usr/bin/env xdg-open\n"
      "Name=Innocent Title\n"
      "Exec=/opt/google/chrome/google-chrome "
      "--app=\"http://evil.com/evil%3B%20rm%20-rf%20/%3B%20%22%3B%20rm%20"
      "-rf%20%24HOME%20%3Eownz0red\"\n"
    },
    { "http://evil.com/evil | cat `echo ownz0red` >/dev/null",
      "Innocent Title",
      "chrome-http__evil.com_evil",

      "Name=Google Chrome\n"
      "Exec=/opt/google/chrome/google-chrome %U\n",

      "#!/usr/bin/env xdg-open\n"
      "Name=Innocent Title\n"
      "Exec=/opt/google/chrome/google-chrome "
      "--app=\"http://evil.com/evil%20%7C%20cat%20%60echo%20ownz0red"
      "%60%20%3E/dev/null\"\n"
    },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); i++) {
    EXPECT_EQ(test_cases[i].expected_output,
              ShellIntegration::GetDesktopFileContents(
                  test_cases[i].template_contents,
                  GURL(test_cases[i].url),
                  ASCIIToUTF16(test_cases[i].title),
                  test_cases[i].icon_name));
  }
}
#endif  // defined(OS_LINUX)
