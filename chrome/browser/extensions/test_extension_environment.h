// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_ENVIRONMENT_H_
#define CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_ENVIRONMENT_H_

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "content/public/test/test_browser_thread_bundle.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/users/scoped_test_user_manager.h"
#include "chrome/browser/chromeos/settings/cros_settings.h"
#include "chrome/browser/chromeos/settings/device_settings_service.h"
#endif

#if defined(OS_WIN)
#include "ui/base/win/scoped_ole_initializer.h"
#endif

class ExtensionService;
class TestingProfile;

namespace base {
class Value;
}

namespace content {
class WebContents;
}

namespace extensions {

class Extension;
class ExtensionPrefs;
class TestExtensionSystem;

// This class provides a minimal environment in which to create
// extensions and tabs for extension-related unittests.
class TestExtensionEnvironment {
 public:
  TestExtensionEnvironment();
  ~TestExtensionEnvironment();

  TestingProfile* profile() const;

  // Returns the TestExtensionSystem created by the TestingProfile.
  TestExtensionSystem* GetExtensionSystem();

  // Returns an ExtensionService created (and owned) by the
  // TestExtensionSystem created by the TestingProfile.
  ExtensionService* GetExtensionService();

  // Returns ExtensionPrefs created (and owned) by the
  // TestExtensionSystem created by the TestingProfile.
  ExtensionPrefs* GetExtensionPrefs();

  // Creates an Extension and registers it with the ExtensionService.
  // The Extension has a default manifest of {name: "Extension",
  // version: "1.0", manifest_version: 2}, and values in
  // manifest_extra override these defaults.
  const Extension* MakeExtension(const base::Value& manifest_extra);

  // Returns a test web contents that has a tab id.
  scoped_ptr<content::WebContents> MakeTab() const;

 private:
  content::TestBrowserThreadBundle thread_bundle_;

#if defined(OS_CHROMEOS)
  chromeos::ScopedTestDeviceSettingsService test_device_settings_service_;
  chromeos::ScopedTestCrosSettings test_cros_settings_;
  chromeos::ScopedTestUserManager test_user_manager_;
#endif

#if defined(OS_WIN)
  ui::ScopedOleInitializer ole_initializer_;
#endif
  scoped_ptr<TestingProfile> profile_;
  ExtensionService* extension_service_;
  ExtensionPrefs* extension_prefs_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_ENVIRONMENT_H_
