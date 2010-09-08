// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_prefs.h"


#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/chrome_thread.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/json_pref_store.h"
#include "testing/gtest/include/gtest/gtest.h"

TestExtensionPrefs::TestExtensionPrefs() {
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  preferences_file_ = temp_dir_.path().AppendASCII("Preferences");
  extensions_dir_ = temp_dir_.path().AppendASCII("Extensions");
  EXPECT_TRUE(file_util::CreateDirectory(extensions_dir_));

  RecreateExtensionPrefs();
}

TestExtensionPrefs::~TestExtensionPrefs() {}

void TestExtensionPrefs::RecreateExtensionPrefs() {
  if (pref_service_.get()) {
    // The PrefService writes its persistent file on the file thread, so we
    // need to wait for any pending I/O to complete before creating a new
    // PrefService.
    MessageLoop file_loop;
    ChromeThread file_thread(ChromeThread::FILE, &file_loop);
    pref_service_->SavePersistentPrefs();
    file_loop.RunAllPending();
  }

  // Create a |PrefService| instance that contains only user defined values.
  pref_service_.reset(PrefService::CreateUserPrefService(preferences_file_));
  ExtensionPrefs::RegisterUserPrefs(pref_service_.get());
  prefs_.reset(new ExtensionPrefs(pref_service_.get(), temp_dir_.path()));
}

Extension* TestExtensionPrefs::AddExtension(std::string name) {
  DictionaryValue dictionary;
  dictionary.SetString(extension_manifest_keys::kName, name);
  dictionary.SetString(extension_manifest_keys::kVersion, "0.1");
  return AddExtensionWithManifest(dictionary);
}

Extension* TestExtensionPrefs::AddExtensionWithManifest(
    const DictionaryValue& manifest) {
  std::string name;
  EXPECT_TRUE(manifest.GetString(extension_manifest_keys::kName, &name));
  FilePath path =  extensions_dir_.AppendASCII(name);
  Extension* extension = new Extension(path);
  std::string errors;
  EXPECT_TRUE(extension->InitFromValue(manifest, false, &errors));
  extension->set_location(Extension::INTERNAL);
  EXPECT_TRUE(Extension::IdIsValid(extension->id()));
  const bool kInitialIncognitoEnabled = false;
  prefs_->OnExtensionInstalled(extension, Extension::ENABLED,
                               kInitialIncognitoEnabled);
  return extension;
}

std::string TestExtensionPrefs::AddExtensionAndReturnId(std::string name) {
  scoped_ptr<Extension> extension(AddExtension(name));
  return extension->id();
}
