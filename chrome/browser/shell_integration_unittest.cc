// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/shell_integration.h"

#include <cstdlib>
#include <map>

#include "base/base_paths.h"
#include "base/file_util.h"
#include "base/files/file_path.h"
#include "base/files/scoped_temp_dir.h"
#include "base/message_loop.h"
#include "base/stl_util.h"
#include "base/string_util.h"
#include "base/test/scoped_path_override.h"
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

  virtual bool GetVar(const char* variable_name, std::string* result) OVERRIDE {
    if (ContainsKey(variables_, variable_name)) {
      *result = variables_[variable_name];
      return true;
    }

    return false;
  }

  virtual bool SetVar(const char* variable_name,
                      const std::string& new_value) OVERRIDE {
    ADD_FAILURE();
    return false;
  }

  virtual bool UnSetVar(const char* variable_name) OVERRIDE {
    ADD_FAILURE();
    return false;
  }

 private:
  std::map<std::string, std::string> variables_;

  DISALLOW_COPY_AND_ASSIGN(MockEnvironment);
};

}  // namespace

TEST(ShellIntegrationTest, GetExistingShortcutLocations) {
  base::FilePath kProfilePath("Default");
  const char kExtensionId[] = "test_extension";
  const char kTemplateFilename[] = "chrome-test_extension-Default.desktop";
  base::FilePath kTemplateFilepath(kTemplateFilename);
  const char kNoDisplayDesktopFile[] = "[Desktop Entry]\nNoDisplay=true";

  MessageLoop message_loop;
  content::TestBrowserThread file_thread(BrowserThread::FILE, &message_loop);

  // No existing shortcuts.
  {
    MockEnvironment env;
    ShellIntegration::ShortcutLocations result =
        ShellIntegrationLinux::GetExistingShortcutLocations(
            &env, kProfilePath, kExtensionId);
    EXPECT_FALSE(result.on_desktop);
    EXPECT_FALSE(result.in_applications_menu);
    EXPECT_FALSE(result.in_quick_launch_bar);
    EXPECT_FALSE(result.hidden);
  }

  // Shortcut on desktop.
  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath desktop_path = temp_dir.path();

    MockEnvironment env;
    ASSERT_TRUE(file_util::CreateDirectory(desktop_path));
    ASSERT_FALSE(file_util::WriteFile(
        desktop_path.AppendASCII(kTemplateFilename),
        "", 0));
    ShellIntegration::ShortcutLocations result =
        ShellIntegrationLinux::GetExistingShortcutLocations(
            &env, kProfilePath, kExtensionId, desktop_path);
    EXPECT_TRUE(result.on_desktop);
    EXPECT_FALSE(result.in_applications_menu);
    EXPECT_FALSE(result.in_quick_launch_bar);
    EXPECT_FALSE(result.hidden);
  }

  // Shortcut in applications directory.
  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath apps_path = temp_dir.path().AppendASCII("applications");

    MockEnvironment env;
    env.Set("XDG_DATA_HOME", temp_dir.path().value());
    ASSERT_TRUE(file_util::CreateDirectory(apps_path));
    ASSERT_FALSE(file_util::WriteFile(
        apps_path.AppendASCII(kTemplateFilename),
        "", 0));
    ShellIntegration::ShortcutLocations result =
        ShellIntegrationLinux::GetExistingShortcutLocations(
            &env, kProfilePath, kExtensionId);
    EXPECT_FALSE(result.on_desktop);
    EXPECT_TRUE(result.in_applications_menu);
    EXPECT_FALSE(result.in_quick_launch_bar);
    EXPECT_FALSE(result.hidden);
  }

  // Shortcut in applications directory with NoDisplay=true.
  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());
    base::FilePath apps_path = temp_dir.path().AppendASCII("applications");

    MockEnvironment env;
    env.Set("XDG_DATA_HOME", temp_dir.path().value());
    ASSERT_TRUE(file_util::CreateDirectory(apps_path));
    ASSERT_TRUE(file_util::WriteFile(
        apps_path.AppendASCII(kTemplateFilename),
        kNoDisplayDesktopFile, strlen(kNoDisplayDesktopFile)));
    ShellIntegration::ShortcutLocations result =
        ShellIntegrationLinux::GetExistingShortcutLocations(
            &env, kProfilePath, kExtensionId);
    // Doesn't count as being in applications menu.
    EXPECT_FALSE(result.on_desktop);
    EXPECT_FALSE(result.in_applications_menu);
    EXPECT_FALSE(result.in_quick_launch_bar);
    EXPECT_TRUE(result.hidden);
  }

  // Shortcut on desktop and in applications directory.
  {
    base::ScopedTempDir temp_dir1;
    ASSERT_TRUE(temp_dir1.CreateUniqueTempDir());
    base::FilePath desktop_path = temp_dir1.path();

    base::ScopedTempDir temp_dir2;
    ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());
    base::FilePath apps_path = temp_dir2.path().AppendASCII("applications");

    MockEnvironment env;
    ASSERT_TRUE(file_util::CreateDirectory(desktop_path));
    ASSERT_FALSE(file_util::WriteFile(
        desktop_path.AppendASCII(kTemplateFilename),
        "", 0));
    env.Set("XDG_DATA_HOME", temp_dir2.path().value());
    ASSERT_TRUE(file_util::CreateDirectory(apps_path));
    ASSERT_FALSE(file_util::WriteFile(
        apps_path.AppendASCII(kTemplateFilename),
        "", 0));
    ShellIntegration::ShortcutLocations result =
        ShellIntegrationLinux::GetExistingShortcutLocations(
            &env, kProfilePath, kExtensionId, desktop_path);
    EXPECT_TRUE(result.on_desktop);
    EXPECT_TRUE(result.in_applications_menu);
    EXPECT_FALSE(result.in_quick_launch_bar);
    EXPECT_FALSE(result.hidden);
  }
}

TEST(ShellIntegrationTest, GetExistingShortcutContents) {
  const char kTemplateFilename[] = "shortcut-test.desktop";
  base::FilePath kTemplateFilepath(kTemplateFilename);
  const char kTestData1[] = "a magical testing string";
  const char kTestData2[] = "a different testing string";

  MessageLoop message_loop;
  content::TestBrowserThread file_thread(BrowserThread::FILE, &message_loop);

  // Test that it searches $XDG_DATA_HOME/applications.
  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

    MockEnvironment env;
    env.Set("XDG_DATA_HOME", temp_dir.path().value());
    // Create a file in a non-applications directory. This should be ignored.
    ASSERT_TRUE(file_util::WriteFile(
        temp_dir.path().AppendASCII(kTemplateFilename),
        kTestData2, strlen(kTestData2)));
    ASSERT_TRUE(file_util::CreateDirectory(
        temp_dir.path().AppendASCII("applications")));
    ASSERT_TRUE(file_util::WriteFile(
        temp_dir.path().AppendASCII("applications")
            .AppendASCII(kTemplateFilename),
        kTestData1, strlen(kTestData1)));
    std::string contents;
    ASSERT_TRUE(
        ShellIntegrationLinux::GetExistingShortcutContents(
            &env, kTemplateFilepath, &contents));
    EXPECT_EQ(kTestData1, contents);
  }

  // Test that it falls back to $HOME/.local/share/applications.
  {
    base::ScopedTempDir temp_dir;
    ASSERT_TRUE(temp_dir.CreateUniqueTempDir());

    MockEnvironment env;
    env.Set("HOME", temp_dir.path().value());
    ASSERT_TRUE(file_util::CreateDirectory(
        temp_dir.path().AppendASCII(".local/share/applications")));
    ASSERT_TRUE(file_util::WriteFile(
        temp_dir.path().AppendASCII(".local/share/applications")
            .AppendASCII(kTemplateFilename),
        kTestData1, strlen(kTestData1)));
    std::string contents;
    ASSERT_TRUE(
        ShellIntegrationLinux::GetExistingShortcutContents(
            &env, kTemplateFilepath, &contents));
    EXPECT_EQ(kTestData1, contents);
  }

  // Test that it searches $XDG_DATA_DIRS/applications.
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
    ASSERT_TRUE(
        ShellIntegrationLinux::GetExistingShortcutContents(
            &env, kTemplateFilepath, &contents));
    EXPECT_EQ(kTestData2, contents);
  }

  // Test that it searches $X/applications for each X in $XDG_DATA_DIRS.
  {
    base::ScopedTempDir temp_dir1;
    ASSERT_TRUE(temp_dir1.CreateUniqueTempDir());
    base::ScopedTempDir temp_dir2;
    ASSERT_TRUE(temp_dir2.CreateUniqueTempDir());

    MockEnvironment env;
    env.Set("XDG_DATA_DIRS", temp_dir1.path().value() + ":" +
                             temp_dir2.path().value());
    // Create a file in a non-applications directory. This should be ignored.
    ASSERT_TRUE(file_util::WriteFile(
        temp_dir1.path().AppendASCII(kTemplateFilename),
        kTestData1, strlen(kTestData1)));
    // Only create a findable desktop file in the second path.
    ASSERT_TRUE(file_util::CreateDirectory(
        temp_dir2.path().AppendASCII("applications")));
    ASSERT_TRUE(file_util::WriteFile(
        temp_dir2.path().AppendASCII("applications")
            .AppendASCII(kTemplateFilename),
        kTestData2, strlen(kTestData2)));
    std::string contents;
    ASSERT_TRUE(
        ShellIntegrationLinux::GetExistingShortcutContents(
            &env, kTemplateFilepath, &contents));
    EXPECT_EQ(kTestData2, contents);
  }
}

TEST(ShellIntegrationTest, GetWebShortcutFilename) {
  const struct {
    const base::FilePath::CharType* path;
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
  const base::FilePath kChromeExePath("/opt/google/chrome/google-chrome");
  const struct {
    const char* url;
    const char* title;
    const char* icon_name;
    bool nodisplay;
    const char* expected_output;
  } test_cases[] = {
    // Real-world case.
    { "http://gmail.com",
      "GMail",
      "chrome-http__gmail.com",
      false,

      "#!/usr/bin/env xdg-open\n"
      "[Desktop Entry]\n"
      "Version=1.0\n"
      "Terminal=false\n"
      "Type=Application\n"
      "Name=GMail\n"
      "Exec=/opt/google/chrome/google-chrome --app=http://gmail.com/\n"
      "Icon=chrome-http__gmail.com\n"
#if !defined(USE_AURA)
      // Aura Chrome does not (yet) set WMClass, so we only expect
      // StartupWMClass on non-Aura builds.
      "StartupWMClass=gmail.com\n"
#endif
    },

    // Make sure that empty icons are replaced by the chrome icon.
    { "http://gmail.com",
      "GMail",
      "",
      false,

      "#!/usr/bin/env xdg-open\n"
      "[Desktop Entry]\n"
      "Version=1.0\n"
      "Terminal=false\n"
      "Type=Application\n"
      "Name=GMail\n"
      "Exec=/opt/google/chrome/google-chrome --app=http://gmail.com/\n"
      "Icon=chromium-browser\n"
#if !defined(USE_AURA)
      // Aura Chrome does not (yet) set WMClass, so we only expect
      // StartupWMClass on non-Aura builds.
      "StartupWMClass=gmail.com\n"
#endif
    },

    // Test adding NoDisplay=true.
    { "http://gmail.com",
      "GMail",
      "chrome-http__gmail.com",
      true,

      "#!/usr/bin/env xdg-open\n"
      "[Desktop Entry]\n"
      "Version=1.0\n"
      "Terminal=false\n"
      "Type=Application\n"
      "Name=GMail\n"
      "Exec=/opt/google/chrome/google-chrome --app=http://gmail.com/\n"
      "Icon=chrome-http__gmail.com\n"
      "NoDisplay=true\n"
#if !defined(USE_AURA)
      // Aura Chrome does not (yet) set WMClass, so we only expect
      // StartupWMClass on non-Aura builds.
      "StartupWMClass=gmail.com\n"
#endif
    },

    // Now we're starting to be more evil...
    { "http://evil.com/evil --join-the-b0tnet",
      "Ownz0red\nExec=rm -rf /",
      "chrome-http__evil.com_evil",
      false,

      "#!/usr/bin/env xdg-open\n"
      "[Desktop Entry]\n"
      "Version=1.0\n"
      "Terminal=false\n"
      "Type=Application\n"
      "Name=http://evil.com/evil%20--join-the-b0tnet\n"
      "Exec=/opt/google/chrome/google-chrome "
      "--app=http://evil.com/evil%20--join-the-b0tnet\n"
      "Icon=chrome-http__evil.com_evil\n"
#if !defined(USE_AURA)
      // Aura Chrome does not (yet) set WMClass, so we only expect
      // StartupWMClass on non-Aura builds.
      "StartupWMClass=evil.com__evil%20--join-the-b0tnet\n"
#endif
    },
    { "http://evil.com/evil; rm -rf /; \"; rm -rf $HOME >ownz0red",
      "Innocent Title",
      "chrome-http__evil.com_evil",
      false,

      "#!/usr/bin/env xdg-open\n"
      "[Desktop Entry]\n"
      "Version=1.0\n"
      "Terminal=false\n"
      "Type=Application\n"
      "Name=Innocent Title\n"
      "Exec=/opt/google/chrome/google-chrome "
      "\"--app=http://evil.com/evil;%20rm%20-rf%20/;%20%22;%20rm%20"
      // Note: $ is escaped as \$ within an arg to Exec, and then
      // the \ is escaped as \\ as all strings in a Desktop file should
      // be; finally, \\ becomes \\\\ when represented in a C++ string!
      "-rf%20\\\\$HOME%20%3Eownz0red\"\n"
      "Icon=chrome-http__evil.com_evil\n"
#if !defined(USE_AURA)
      // Aura Chrome does not (yet) set WMClass, so we only expect
      // StartupWMClass on non-Aura builds.
      "StartupWMClass=evil.com__evil;%20rm%20-rf%20_;%20%22;%20"
      "rm%20-rf%20$HOME%20%3Eownz0red\n"
#endif
    },
    { "http://evil.com/evil | cat `echo ownz0red` >/dev/null",
      "Innocent Title",
      "chrome-http__evil.com_evil",
      false,

      "#!/usr/bin/env xdg-open\n"
      "[Desktop Entry]\n"
      "Version=1.0\n"
      "Terminal=false\n"
      "Type=Application\n"
      "Name=Innocent Title\n"
      "Exec=/opt/google/chrome/google-chrome "
      "--app=http://evil.com/evil%20%7C%20cat%20%60echo%20ownz0red"
      "%60%20%3E/dev/null\n"
      "Icon=chrome-http__evil.com_evil\n"
#if !defined(USE_AURA)
      // Aura Chrome does not (yet) set WMClass, so we only expect
      // StartupWMClass on non-Aura builds.
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
            kChromeExePath,
            web_app::GenerateApplicationNameFromURL(GURL(test_cases[i].url)),
            GURL(test_cases[i].url),
            std::string(),
            base::FilePath(),
            ASCIIToUTF16(test_cases[i].title),
            test_cases[i].icon_name,
            base::FilePath(),
            test_cases[i].nodisplay));
  }
}

TEST(ShellIntegrationTest, GetDirectoryFileContents) {
  const struct {
    const char* title;
    const char* icon_name;
    const char* expected_output;
  } test_cases[] = {
    // Real-world case.
    { "Chrome Apps",
      "chrome-apps",

      "[Desktop Entry]\n"
      "Version=1.0\n"
      "Type=Directory\n"
      "Name=Chrome Apps\n"
      "Icon=chrome-apps\n"
    },

    // Make sure that empty icons are replaced by the chrome icon.
    { "Chrome Apps",
      "",

      "[Desktop Entry]\n"
      "Version=1.0\n"
      "Type=Directory\n"
      "Name=Chrome Apps\n"
      "Icon=chromium-browser\n"
    },
  };

  for (size_t i = 0; i < ARRAYSIZE_UNSAFE(test_cases); i++) {
    SCOPED_TRACE(i);
    EXPECT_EQ(
        test_cases[i].expected_output,
        ShellIntegrationLinux::GetDirectoryFileContents(
            ASCIIToUTF16(test_cases[i].title),
            test_cases[i].icon_name));
  }
}

#endif
