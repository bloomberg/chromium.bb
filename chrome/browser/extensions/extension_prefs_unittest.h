// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_UNITTEST_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_UNITTEST_H_

#include "base/message_loop.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "chrome/common/extensions/extension_unittest.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

class PrefRegistrySyncable;

namespace base {
class Value;
}

namespace extensions {
class Extension;

// Base class for extension preference-related unit tests.
class ExtensionPrefsTest : public ExtensionTest {
 public:
  ExtensionPrefsTest();
  virtual ~ExtensionPrefsTest();

  // This function will get called once, and is the right place to do operations
  // on ExtensionPrefs that write data.
  virtual void Initialize() = 0;

  // This function will be called twice - once while the original ExtensionPrefs
  // object is still alive, and once after recreation. Thus, it tests that
  // things don't break after any ExtensionPrefs startup work.
  virtual void Verify() = 0;

  // This function is called to Register preference default values.
  virtual void RegisterPreferences(PrefRegistrySyncable* registry);

  virtual void SetUp() OVERRIDE;

  virtual void TearDown() OVERRIDE;

 protected:
  ExtensionPrefs* prefs() { return prefs_.prefs(); }

  MessageLoop message_loop_;
  content::TestBrowserThread ui_thread_;

  TestExtensionPrefs prefs_;

 private:
  DISALLOW_COPY_AND_ASSIGN(ExtensionPrefsTest);
};

class ExtensionPrefsPrepopulatedTest : public ExtensionPrefsTest {
 public:
  ExtensionPrefsPrepopulatedTest();
  virtual ~ExtensionPrefsPrepopulatedTest();

  virtual void RegisterPreferences(PrefRegistrySyncable* registry) OVERRIDE;

  void InstallExtControlledPref(Extension* ext,
                                const std::string& key,
                                base::Value* val);

  void InstallExtControlledPrefIncognito(Extension* ext,
                                         const std::string& key,
                                         base::Value* val);

  void InstallExtControlledPrefIncognitoSessionOnly(
      Extension* ext,
      const std::string& key,
      base::Value* val);

  void InstallExtension(Extension* ext);

  void UninstallExtension(const std::string& extension_id);

  // Weak references, for convenience.
  Extension* ext1_;
  Extension* ext2_;
  Extension* ext3_;
  Extension* ext4_;

  // Flags indicating whether each of the extensions has been installed, yet.
  bool installed[4];

 private:
  void EnsureExtensionInstalled(Extension *ext);

  void EnsureExtensionUninstalled(const std::string& extension_id);

  scoped_refptr<Extension> ext1_scoped_;
  scoped_refptr<Extension> ext2_scoped_;
  scoped_refptr<Extension> ext3_scoped_;
  scoped_refptr<Extension> ext4_scoped_;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_PREFS_UNITTEST_H_
