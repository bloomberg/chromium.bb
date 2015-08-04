// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_ENVIRONMENT_H_
#define CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_ENVIRONMENT_H_

#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"

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
class TestBrowserThreadBundle;
}

namespace extensions {

class Extension;
class ExtensionPrefs;
class TestExtensionSystem;

// This class provides a minimal environment in which to create
// extensions and tabs for extension-related unittests.
class TestExtensionEnvironment {
 public:
  // Fetches the TestExtensionSystem in |profile| and creates a default
  // ExtensionService there,
  static ExtensionService* CreateExtensionServiceForProfile(
      TestingProfile* profile);

  TestExtensionEnvironment();

  // Allows a test harness to pass its own message loop (typically
  // base::MessageLoopForUI::current()), rather than have
  // TestExtensionEnvironment create and own a TestBrowserThreadBundle.
  explicit TestExtensionEnvironment(base::MessageLoopForUI* message_loop);

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

  // Use a specific extension ID instead of the default generated in
  // Extension::Create.
  const Extension* MakeExtension(const base::Value& manifest_extra,
                                 const std::string& id);

  // Generates a valid packaged app manifest with the given ID. If |install|
  // it gets added to the ExtensionService in |profile|.
  scoped_refptr<Extension> MakePackagedApp(const std::string& id, bool install);

  // Returns a test web contents that has a tab id.
  scoped_ptr<content::WebContents> MakeTab() const;

  // Deletes the testing profile to test profile teardown.
  void DeleteProfile();

 private:
  class ChromeOSEnv;

  void Init();

  scoped_ptr<content::TestBrowserThreadBundle> thread_bundle_;
  scoped_ptr<ChromeOSEnv> chromeos_env_;

#if defined(OS_WIN)
  ui::ScopedOleInitializer ole_initializer_;
#endif
  scoped_ptr<TestingProfile> profile_;
  ExtensionService* extension_service_;

  DISALLOW_COPY_AND_ASSIGN(TestExtensionEnvironment);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_ENVIRONMENT_H_
