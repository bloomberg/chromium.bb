// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "extensions/shell/browser/shell_prefs.h"

#include "base/path_service.h"
#include "base/prefs/pref_service.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/test_browser_context.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "extensions/common/extension_paths.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// A BrowserContext that uses a test data directory as its data path.
class PrefsTestBrowserContext : public content::TestBrowserContext {
 public:
  PrefsTestBrowserContext() {}
  ~PrefsTestBrowserContext() override {}

  // content::BrowserContext:
  base::FilePath GetPath() const override {
    base::FilePath path;
    PathService::Get(extensions::DIR_TEST_DATA, &path);
    return path.AppendASCII("shell_prefs");
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(PrefsTestBrowserContext);
};

}  // namespace

namespace extensions {

TEST(ShellPrefsTest, CreatePrefService) {
  content::TestBrowserThreadBundle thread_bundle;
  PrefsTestBrowserContext browser_context;

  // Create the pref service. This loads the test pref file.
  scoped_ptr<PrefService> service =
      ShellPrefs::CreatePrefService(&browser_context);

  // Some basic extension preferences are registered.
  EXPECT_TRUE(service->FindPreference("extensions.settings"));
  EXPECT_TRUE(service->FindPreference("extensions.toolbarsize"));
  EXPECT_FALSE(service->FindPreference("should.not.exist"));

  // User prefs from the file have been read correctly.
  EXPECT_EQ("1.2.3.4", service->GetString("extensions.last_chrome_version"));
  EXPECT_EQ(123, service->GetInteger("extensions.toolbarsize"));

  // The user prefs system has been initialized.
  EXPECT_EQ(service.get(), user_prefs::UserPrefs::Get(&browser_context));
}

}  // namespace extensions
