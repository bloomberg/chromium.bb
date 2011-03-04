// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_prefs.h"

#include "base/file_util.h"
#include "base/message_loop.h"
#include "base/message_loop_proxy.h"
#include "base/scoped_ptr.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/browser/extensions/extension_pref_value_map.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_service_mock_builder.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/common/extensions/extension_constants.h"
#include "chrome/common/json_pref_store.h"
#include "content/browser/browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

// Mock ExtensionPrefs class with artificial clock to guarantee that no two
// extensions get the same installation time stamp and we can reliably
// assert the installation order in the tests below.
class MockExtensionPrefs : public ExtensionPrefs {
 public:
  MockExtensionPrefs(PrefService* prefs,
                     const FilePath& root_dir,
                     ExtensionPrefValueMap* extension_pref_value_map)
    : ExtensionPrefs(prefs, root_dir, extension_pref_value_map),
      currentTime(base::Time::Now()) {}
  ~MockExtensionPrefs() {}

 protected:
  mutable base::Time currentTime;

  virtual base::Time GetCurrentTime() const {
    currentTime += base::TimeDelta::FromSeconds(10);
    return currentTime;
  }
};

}  // namespace

TestExtensionPrefs::TestExtensionPrefs() : pref_service_(NULL) {
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  preferences_file_ = temp_dir_.path().AppendASCII("Preferences");
  extensions_dir_ = temp_dir_.path().AppendASCII("Extensions");
  EXPECT_TRUE(file_util::CreateDirectory(extensions_dir_));

  RecreateExtensionPrefs();
}

TestExtensionPrefs::~TestExtensionPrefs() {}

void TestExtensionPrefs::RecreateExtensionPrefs() {
  // We persist and reload the PrefService's PrefStores because this process
  // deletes all empty dictionaries. The ExtensionPrefs implementation
  // needs to be able to handle this situation.
  if (pref_service_.get()) {
    // The PrefService writes its persistent file on the file thread, so we
    // need to wait for any pending I/O to complete before creating a new
    // PrefService.
    MessageLoop file_loop;
    BrowserThread file_thread(BrowserThread::FILE, &file_loop);
    pref_service_->SavePersistentPrefs();
    file_loop.RunAllPending();
  }

  extension_pref_value_map_.reset(new ExtensionPrefValueMap);
  PrefServiceMockBuilder builder;
  builder.WithUserFilePrefs(preferences_file_);
  builder.WithExtensionPrefs(
      new ExtensionPrefStore(extension_pref_value_map_.get(), false));
  pref_service_.reset(builder.Create());
  ExtensionPrefs::RegisterUserPrefs(pref_service_.get());

  prefs_.reset(new MockExtensionPrefs(pref_service_.get(),
                                      temp_dir_.path(),
                                      extension_pref_value_map_.get()));
}

scoped_refptr<Extension> TestExtensionPrefs::AddExtension(std::string name) {
  DictionaryValue dictionary;
  dictionary.SetString(extension_manifest_keys::kName, name);
  dictionary.SetString(extension_manifest_keys::kVersion, "0.1");
  return AddExtensionWithManifest(dictionary, Extension::INTERNAL);
}

scoped_refptr<Extension> TestExtensionPrefs::AddExtensionWithManifest(
    const DictionaryValue& manifest, Extension::Location location) {
  std::string name;
  EXPECT_TRUE(manifest.GetString(extension_manifest_keys::kName, &name));
  FilePath path =  extensions_dir_.AppendASCII(name);
  std::string errors;
  scoped_refptr<Extension> extension = Extension::Create(
      path, location, manifest, false, true, &errors);
  EXPECT_TRUE(extension);
  if (!extension)
    return NULL;

  EXPECT_TRUE(Extension::IdIsValid(extension->id()));
  const bool kInitialIncognitoEnabled = false;
  prefs_->OnExtensionInstalled(extension, Extension::ENABLED,
                               kInitialIncognitoEnabled);
  return extension;
}

std::string TestExtensionPrefs::AddExtensionAndReturnId(std::string name) {
  scoped_refptr<Extension> extension(AddExtension(name));
  return extension->id();
}

PrefService* TestExtensionPrefs::CreateIncognitoPrefService() const {
  return pref_service_->CreateIncognitoPrefService(
      new ExtensionPrefStore(extension_pref_value_map_.get(), true));
}
