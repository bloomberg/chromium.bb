// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include <map>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/web_applications/web_app.h"
#include "chrome/common/chrome_constants.h"
#include "content/public/test/test_browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_POSIX) && !defined(OS_MACOSX)
#include "base/environment.h"
#include "chrome/browser/shell_integration_linux.h"
#endif

#define FPL FILE_PATH_LITERAL

using content::BrowserThread;

#if defined(OS_POSIX) && !defined(OS_MACOSX)
namespace {

// Provides mock environment variables values based on a stored map.
class MockEnvironment : public base::Environment {
 public:
  MockEnvironment() {}

  void Set(const std::string& name, const std::string& value) {
    variables_[name] = value;
  }

  virtual bool GetVar(const char* variable_name, std::string* result) {
    if (ContainsKey(variables_, variable_name)) {
      *result = variables_[variable_name];
      return true;
    }

    return false;
  }

  virtual bool SetVar(const char* variable_name, const std::string& new_value) {
    ADD_FAILURE();
    return false;
  }

  virtual bool UnSetVar(const char* variable_name) {
    ADD_FAILURE();
    return false;
  }

 private:
  std::map<std::string, std::string> variables_;

  DISALLOW_COPY_AND_ASSIGN(MockEnvironment);
};

}  // namespace

TEST(ShellIntegrationTest, GetDesktopShortcutTemplate) {
#if defined(GOOGLE_CHROME_BUILD)
  const char kTemplateFilename[] = "google-chrome.desktop";
#else  // CHROMIUM_BUILD
  const char kTemplateFilename[] = "chromium-browser.desktop";
#endif

  const char kTestData1[] = "a magical testing string";
  const char kTestData2[] = "a different testing string";

  MessageLoop message_loop;
  content::TestBrowserThread file_thread(BrowserThread::FILE, &message_loop);

  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

    MockEnvironment env;
    env.Set("XDG_DATA_HOME", temp_dir.path().value());
    ASSERT_TRUE(file_util::WriteFile(
        temp_dir.path().AppendASCII(kTemplateFilename),
        kTestData1, strlen(kTestData1)));
    std::string contents;
    ASSERT_TRUE(ShellIntegrationLinux::GetDesktopShortcutTemplate(&env,
                                                                  &contents));
    EXPECT_EQ(kTestData1, contents);
  }

  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

    MockEnvironment env;
    env.Set("XDG_DATA_DIRS", temp_dir.path().value());
    ASSERT_TRUE(file_util::CreateDirectory(
        temp_dir.path().AppendASCII("applications")));
    ASSERT_TRUE(file_util::WriteFile(
        temp_dir.path().AppendASCII("applications")
            .AppendASCII(kTemplateFilename),
        kTestData2, strlen(kTestData2)));
    std::string contents;
    ASSERT_TRUE(ShellIntegrationLinux::GetDesktopShortcutTemplate(&env,
                                                                  &contents));
    EXPECT_EQ(kTestData2, contents);
  }

  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

    MockEnvironment env;
    env.Set("XDG_DATA_DIRS", temp_dir.path().value() + ":" +
                   temp_dir.path().AppendASCII("applications").value());
    ASSERT_TRUE(file_util::CreateDirectory(
        temp_dir.path().AppendASCII("applications")));
    ASSERT_TRUE(file_util::WriteFile(
        temp_dir.path().AppendASCII(kTemplateFilename),
        kTestData1, strlen(kTestData1)));
    ASSERT_TRUE(file_util::WriteFile(
        temp_dir.path().AppendASCII("applications")
            .AppendASCII(kTemplateFilename),
        kTestData2, strlen(kTestData2)));
    std::string contents;
    ASSERT_TRUE(ShellIntegrationLinux::GetDesktopShortcutTemplate(&env,
                                                                  &contents));
    EXPECT_EQ(kTestData1, contents);
  }
}

TEST(ShellIntegrationTest, GetWebShortcutFilename) {
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
    EXPECT_EQ(std::string(chrome::kBrowserProcessExecutableName) + "-" +
              test_cases[i].path,
              ShellIntegrationLinux::GetWebShortcutFilename(
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
      "MimeType=text/html;text/xml;application/xhtml_xml;\n"
      "X-Ayatana-Desktop-Shortcuts=NewWindow;\n"
      "\n"
      "[NewWindow Shortcut Group]\n"
      "Name=Open New Window\n"
      "Exec=/opt/google/chrome/google-chrome\n"
      "TargetEnvironment=Unity\n",

      "#!/usr/bin/env xdg-open\n"
      "[Desktop Entry]\n"
      "Version=1.0\n"
      "Encoding=UTF-8\n"
      "Name=GMail\n"
      "Exec=/opt/google/chrome/google-chrome --app=http://gmail.com/\n"
      "Terminal=false\n"
      "Icon=chrome-http__gmail.com\n"
      "Type=Application\n"
      "Categories=Application;Network;WebBrowser;\n"
#if !defined(USE_AURA)
      // Aura Chrome creates browser window in a single X11 window, so
      // WMClass does not matter.
      "StartupWMClass=gmail.com\n"
#endif
    },

    // Make sure we don't insert duplicate shebangs.
    { "http://gmail.com",
      "GMail",
      "chrome-http__gmail.com",

      "#!/some/shebang\n"
      "[Desktop Entry]\n"
      "Name=Google Chrome\n"
      "Exec=/opt/google/chrome/google-chrome %U\n",

      "#!/usr/bin/env xdg-open\n"
      "[Desktop Entry]\n"
      "Name=GMail\n"
      "Exec=/opt/google/chrome/google-chrome --app=http://gmail.com/\n"
      "Icon=chrome-http__gmail.com\n"
#if !defined(USE_AURA)
      // Aura Chrome creates browser window in a single X11 window, so
      // WMClass does not matter.
      "StartupWMClass=gmail.com\n"
#endif
    },

    // Make sure i18n-ed comments are removed.
    { "http://gmail.com",
      "GMail",
      "chrome-http__gmail.com",

      "[Desktop Entry]\n"
      "Name=Google Chrome\n"
      "Exec=/opt/google/chrome/google-chrome %U\n"
      "Comment[pl]=Jakis komentarz.\n",

      "#!/usr/bin/env xdg-open\n"
      "[Desktop Entry]\n"
      "Name=GMail\n"
      "Exec=/opt/google/chrome/google-chrome --app=http://gmail.com/\n"
      "Icon=chrome-http__gmail.com\n"
#if !defined(USE_AURA)
      // Aura Chrome creates browser window in a single X11 window, so
      // WMClass does not matter.
      "StartupWMClass=gmail.com\n"
#endif
    },

    // Make sure that empty icons are replaced by the chrome icon.
    { "http://gmail.com",
      "GMail",
      "",

      "[Desktop Entry]\n"
      "Name=Google Chrome\n"
      "Exec=/opt/google/chrome/google-chrome %U\n"
      "Comment[pl]=Jakis komentarz.\n"
      "Icon=/opt/google/chrome/product_logo_48.png\n",

      "#!/usr/bin/env xdg-open\n"
      "[Desktop Entry]\n"
      "Name=GMail\n"
      "Exec=/opt/google/chrome/google-chrome --app=http://gmail.com/\n"
      "Icon=/opt/google/chrome/product_logo_48.png\n"
#if !defined(USE_AURA)
      // Aura Chrome creates browser window in a single X11 window, so
      // WMClass does not matter.
      "StartupWMClass=gmail.com\n"
#endif
    },

    // Now we're starting to be more evil...
    { "http://evil.com/evil --join-the-b0tnet",
      "Ownz0red\nExec=rm -rf /",
      "chrome-http__evil.com_evil",

      "[Desktop Entry]\n"
      "Name=Google Chrome\n"
      "Exec=/opt/google/chrome/google-chrome %U\n",

      "#!/usr/bin/env xdg-open\n"
      "[Desktop Entry]\n"
      "Name=http://evil.com/evil%20--join-the-b0tnet\n"
      "Exec=/opt/google/chrome/google-chrome "
      "--app=http://evil.com/evil%20--join-the-b0tnet\n"
      "Icon=chrome-http__evil.com_evil\n"
#if !defined(USE_AURA)
      // Aura Chrome creates browser window in a single X11 window, so
      // WMClass does not matter.
      "StartupWMClass=evil.com__evil%20--join-the-b0tnet\n"
#endif
    },
    { "http://evil.com/evil; rm -rf /; \"; rm -rf $HOME >ownz0red",
      "Innocent Title",
      "chrome-http__evil.com_evil",

      "[Desktop Entry]\n"
      "Name=Google Chrome\n"
      "Exec=/opt/google/chrome/google-chrome %U\n",

      "#!/usr/bin/env xdg-open\n"
      "[Desktop Entry]\n"
      "Name=Innocent Title\n"
      "Exec=/opt/google/chrome/google-chrome "
      "\"--app=http://evil.com/evil;%20rm%20-rf%20/;%20%22;%20rm%20"
      // Note: $ is escaped as \$ within an arg to Exec, and then
      // the \ is escaped as \\ as all strings in a Desktop file should
      // be; finally, \\ becomes \\\\ when represented in a C++ string!
      "-rf%20\\\\$HOME%20%3Eownz0red\"\n"
      "Icon=chrome-http__evil.com_evil\n"
#if !defined(USE_AURA)
      // Aura Chrome creates browser window in a single X11 window, so
      // WMClass does not matter.
      "StartupWMClass=evil.com__evil;%20rm%20-rf%20_;%20%22;%20"
      "rm%20-rf%20$HOME%20%3Eownz0red\n"
#endif
    },
    { "http://evil.com/evil | cat `echo ownz0red` >/dev/null",
      "Innocent Title",
      "chrome-http__evil.com_evil",

      "[Desktop Entry]\n"
      "Name=Google Chrome\n"
      "Exec=/opt/google/chrome/google-chrome %U\n",

      "#!/usr/bin/env xdg-open\n"
      "[Desktop Entry]\n"
      "Name=Innocent Title\n"
      "Exec=/opt/google/chrome/google-chrome "
      "--app=http://evil.com/evil%20%7C%20cat%20%60echo%20ownz0red"
      "%60%20%3E/dev/null\n"
      "Icon=chrome-http__evil.com_evil\n"
#if !defined(USE_AURA)
      // Aura Chrome creates browser window in a single X11 window, so
      // WMClass does not matter.
      "StartupWMClass=evil.com__evil%20%7C%20cat%20%60echo%20ownz0red"
      "%60%20%3E_dev_null\n"
#endif
    },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); i++) {
    SCOPED_TRACE(i);
    EXPECT_EQ(
        test_cases[i].expected_output,
        ShellIntegrationLinux::GetDesktopFileContents(
            test_cases[i].template_contents,
            web_app::GenerateApplicationNameFromURL(GURL(test_cases[i].url)),
            GURL(test_cases[i].url),
            "",
            FilePath(),
            ASCIIToUTF16(test_cases[i].title),
            test_cases[i].icon_name,
            FilePath()));
  }
}
#endif
