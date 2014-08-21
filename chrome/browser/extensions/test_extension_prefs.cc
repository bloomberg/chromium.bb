// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/test_extension_prefs.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/file_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/message_loop/message_loop.h"
#include "base/message_loop/message_loop_proxy.h"
#include "base/prefs/json_pref_store.h"
#include "base/prefs/pref_value_store.h"
#include "base/run_loop.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/values.h"
#include "chrome/browser/prefs/pref_service_mock_factory.h"
#include "chrome/browser/prefs/pref_service_syncable.h"
#include "chrome/common/chrome_constants.h"
#include "components/crx_file/id_util.h"
#include "components/pref_registry/pref_registry_syncable.h"
#include "content/public/browser/browser_thread.h"
#include "extensions/browser/extension_pref_store.h"
#include "extensions/browser/extension_pref_value_map.h"
#include "extensions/browser/extension_prefs.h"
#include "extensions/browser/extensions_browser_client.h"
#include "extensions/common/extension.h"
#include "extensions/common/manifest_constants.h"
#include "sync/api/string_ordinal.h"
#include "testing/gtest/include/gtest/gtest.h"

using content::BrowserThread;

namespace extensions {

namespace {

// A TimeProvider which returns an incrementally later time each time
// GetCurrentTime is called.
class IncrementalTimeProvider : public ExtensionPrefs::TimeProvider {
 public:
  IncrementalTimeProvider() : current_time_(base::Time::Now()) {
  }

  virtual ~IncrementalTimeProvider() {
  }

  virtual base::Time GetCurrentTime() const OVERRIDE {
    current_time_ += base::TimeDelta::FromSeconds(10);
    return current_time_;
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(IncrementalTimeProvider);

  mutable base::Time current_time_;
};

}  // namespace

TestExtensionPrefs::TestExtensionPrefs(base::SequencedTaskRunner* task_runner)
    : task_runner_(task_runner), extensions_disabled_(false) {
  EXPECT_TRUE(temp_dir_.CreateUniqueTempDir());
  preferences_file_ = temp_dir_.path().Append(chrome::kPreferencesFilename);
  extensions_dir_ = temp_dir_.path().AppendASCII("Extensions");
  EXPECT_TRUE(base::CreateDirectory(extensions_dir_));

  ResetPrefRegistry();
  RecreateExtensionPrefs();
}

TestExtensionPrefs::~TestExtensionPrefs() {
}

PrefService* TestExtensionPrefs::pref_service() {
  return pref_service_.get();
}

const scoped_refptr<user_prefs::PrefRegistrySyncable>&
TestExtensionPrefs::pref_registry() {
  return pref_registry_;
}

void TestExtensionPrefs::ResetPrefRegistry() {
  pref_registry_ = new user_prefs::PrefRegistrySyncable;
  ExtensionPrefs::RegisterProfilePrefs(pref_registry_.get());
}

void TestExtensionPrefs::RecreateExtensionPrefs() {
  // We persist and reload the PrefService's PrefStores because this process
  // deletes all empty dictionaries. The ExtensionPrefs implementation
  // needs to be able to handle this situation.
  if (pref_service_) {
    // Commit a pending write (which posts a task to task_runner_) and wait for
    // it to finish.
    pref_service_->CommitPendingWrite();
    base::RunLoop run_loop;
    ASSERT_TRUE(
        task_runner_->PostTaskAndReply(
            FROM_HERE,
            base::Bind(&base::DoNothing),
            run_loop.QuitClosure()));
    run_loop.Run();
  }

  extension_pref_value_map_.reset(new ExtensionPrefValueMap);
  PrefServiceMockFactory factory;
  factory.SetUserPrefsFile(preferences_file_, task_runner_.get());
  factory.set_extension_prefs(
      new ExtensionPrefStore(extension_pref_value_map_.get(), false));
  pref_service_ = factory.CreateSyncable(pref_registry_.get()).Pass();

  prefs_.reset(ExtensionPrefs::Create(
      pref_service_.get(),
      temp_dir_.path(),
      extension_pref_value_map_.get(),
      ExtensionsBrowserClient::Get()->CreateAppSorting().Pass(),
      extensions_disabled_,
      std::vector<ExtensionPrefsObserver*>(),
      // Guarantee that no two extensions get the same installation time
      // stamp and we can reliably assert the installation order in the tests.
      scoped_ptr<ExtensionPrefs::TimeProvider>(new IncrementalTimeProvider())));
}

scoped_refptr<Extension> TestExtensionPrefs::AddExtension(
    const std::string& name) {
  base::DictionaryValue dictionary;
  dictionary.SetString(manifest_keys::kName, name);
  dictionary.SetString(manifest_keys::kVersion, "0.1");
  return AddExtensionWithManifest(dictionary, Manifest::INTERNAL);
}

scoped_refptr<Extension> TestExtensionPrefs::AddApp(const std::string& name) {
  base::DictionaryValue dictionary;
  dictionary.SetString(manifest_keys::kName, name);
  dictionary.SetString(manifest_keys::kVersion, "0.1");
  dictionary.SetString(manifest_keys::kApp, "true");
  dictionary.SetString(manifest_keys::kLaunchWebURL, "http://example.com");
  return AddExtensionWithManifest(dictionary, Manifest::INTERNAL);

}

scoped_refptr<Extension> TestExtensionPrefs::AddExtensionWithManifest(
    const base::DictionaryValue& manifest, Manifest::Location location) {
  return AddExtensionWithManifestAndFlags(manifest, location,
                                          Extension::NO_FLAGS);
}

scoped_refptr<Extension> TestExtensionPrefs::AddExtensionWithManifestAndFlags(
    const base::DictionaryValue& manifest,
    Manifest::Location location,
    int extra_flags) {
  std::string name;
  EXPECT_TRUE(manifest.GetString(manifest_keys::kName, &name));
  base::FilePath path =  extensions_dir_.AppendASCII(name);
  std::string errors;
  scoped_refptr<Extension> extension = Extension::Create(
      path, location, manifest, extra_flags, &errors);
  EXPECT_TRUE(extension.get()) << errors;
  if (!extension.get())
    return NULL;

  EXPECT_TRUE(crx_file::id_util::IdIsValid(extension->id()));
  prefs_->OnExtensionInstalled(extension.get(),
                               Extension::ENABLED,
                               syncer::StringOrdinal::CreateInitialOrdinal(),
                               std::string());
  return extension;
}

std::string TestExtensionPrefs::AddExtensionAndReturnId(
    const std::string& name) {
  scoped_refptr<Extension> extension(AddExtension(name));
  return extension->id();
}

PrefService* TestExtensionPrefs::CreateIncognitoPrefService() const {
  return pref_service_->CreateIncognitoPrefService(
      new ExtensionPrefStore(extension_pref_value_map_.get(), true));
}

void TestExtensionPrefs::set_extensions_disabled(bool extensions_disabled) {
  extensions_disabled_ = extensions_disabled;
}

}  // namespace extensions
