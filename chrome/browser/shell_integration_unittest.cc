// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include <map>

#include "base/file_path.h"
#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/scoped_temp_dir.h"
#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_paths_internal.h"
#include "content/browser/browser_thread.h"
#include "googleurl/src/gurl.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "chrome/installer/util/browser_distribution.h"
#elif defined(OS_LINUX)
#include "base/environment.h"
#endif  // defined(OS_LINUX)

#define FPL FILE_PATH_LITERAL

#if defined(OS_LINUX)
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
  BrowserThread file_thread(BrowserThread::FILE, &message_loop);

  {
    ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

    MockEnvironment env;
    env.Set("XDG_DATA_HOME", temp_dir.path().value());
    ASSERT_TRUE(file_util::WriteFile(
        temp_dir.path().AppendASCII(kTemplateFilename),
        kTestData1, strlen(kTestData1)));
    std::string contents;
    ASSERT_TRUE(ShellIntegration::GetDesktopShortcutTemplate(&env,
                                                             &contents));
    EXPECT_EQ(kTestData1, contents);
  }

  {
    ScopedTempDir temp_dir;
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
    ASSERT_TRUE(ShellIntegration::GetDesktopShortcutTemplate(&env,
                                                             &contents));
    EXPECT_EQ(kTestData2, contents);
  }

  {
    ScopedTempDir temp_dir;
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
    ASSERT_TRUE(ShellIntegration::GetDesktopShortcutTemplate(&env,
                                                             &contents));
    EXPECT_EQ(kTestData1, contents);
  }
}

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
    EXPECT_EQ(std::string(chrome::kBrowserProcessExecutableName) + "-" +
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
      "Exec=/opt/google/chrome/google-chrome --app=http://gmail.com/\n"
      "Terminal=false\n"
      "Icon=chrome-http__gmail.com\n"
      "Type=Application\n"
      "Categories=Application;Network;WebBrowser;\n"
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
      "Exec=/opt/google/chrome/google-chrome --app=http://gmail.com/\n"
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
      "Exec=/opt/google/chrome/google-chrome --app=http://gmail.com/\n"
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
      "Exec=/opt/google/chrome/google-chrome --app=http://gmail.com/\n"
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
      "--app=http://evil.com/evil%20--join-the-b0tnet\n"
    },
    { "http://evil.com/evil; rm -rf /; \"; rm -rf $HOME >ownz0red",
      "Innocent Title",
      "chrome-http__evil.com_evil",

      "Name=Google Chrome\n"
      "Exec=/opt/google/chrome/google-chrome %U\n",

      "#!/usr/bin/env xdg-open\n"
      "Name=Innocent Title\n"
      "Exec=/opt/google/chrome/google-chrome "
      "\"--app=http://evil.com/evil;%20rm%20-rf%20/;%20%22;%20rm%20"
      // Note: $ is escaped as \$ within an arg to Exec, and then
      // the \ is escaped as \\ as all strings in a Desktop file should
      // be; finally, \\ becomes \\\\ when represented in a C++ string!
      "-rf%20\\\\$HOME%20%3Eownz0red\"\n"
    },
    { "http://evil.com/evil | cat `echo ownz0red` >/dev/null",
      "Innocent Title",
      "chrome-http__evil.com_evil",

      "Name=Google Chrome\n"
      "Exec=/opt/google/chrome/google-chrome %U\n",

      "#!/usr/bin/env xdg-open\n"
      "Name=Innocent Title\n"
      "Exec=/opt/google/chrome/google-chrome "
      "--app=http://evil.com/evil%20%7C%20cat%20%60echo%20ownz0red"
      "%60%20%3E/dev/null\n"
    },
  };
  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); i++) {
    SCOPED_TRACE(i);
    EXPECT_EQ(test_cases[i].expected_output,
              ShellIntegration::GetDesktopFileContents(
                  test_cases[i].template_contents,
                  GURL(test_cases[i].url),
                  "",
                  ASCIIToUTF16(test_cases[i].title),
                  test_cases[i].icon_name));
  }
}
#elif defined(OS_WIN)
TEST(ShellIntegrationTest, GetChromiumAppIdTest) {
  // Empty profile path should get chrome::kBrowserAppID
  FilePath empty_path;
  EXPECT_EQ(BrowserDistribution::GetDistribution()->GetBrowserAppId(),
            ShellIntegration::GetChromiumAppId(empty_path));

  // Default profile path should get chrome::kBrowserAppID
  FilePath default_user_data_dir;
  chrome::GetDefaultUserDataDirectory(&default_user_data_dir);
  FilePath default_profile_path =
      default_user_data_dir.AppendASCII(chrome::kNotSignedInProfile);
  EXPECT_EQ(BrowserDistribution::GetDistribution()->GetBrowserAppId(),
            ShellIntegration::GetChromiumAppId(default_profile_path));

  // Non-default profile path should get chrome::kBrowserAppID joined with
  // profile info.
  FilePath profile_path(FILE_PATH_LITERAL("root"));
  profile_path = profile_path.Append(FILE_PATH_LITERAL("udd"));
  profile_path = profile_path.Append(FILE_PATH_LITERAL("User Data - Test"));
  EXPECT_EQ(BrowserDistribution::GetDistribution()->GetBrowserAppId() +
            L".udd.UserDataTest",
            ShellIntegration::GetChromiumAppId(profile_path));
}
#endif
