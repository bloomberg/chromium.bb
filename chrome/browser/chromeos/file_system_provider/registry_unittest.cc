// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/registry.h"

#include <string>
#include <vector>

#include "base/files/file_path.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "chrome/browser/chromeos/file_system_provider/registry.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chrome/test/base/testing_pref_service_syncable.h"
#include "chrome/test/base/testing_profile.h"
#include "chrome/test/base/testing_profile_manager.h"
#include "components/user_prefs/user_prefs.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {
namespace file_system_provider {
namespace {

const char kExtensionId[] = "mbflcebpggnecokmikipoihdbecnjfoj";
const char kDisplayName[] = "Camera Pictures";

// The dot in the file system ID is there in order to check that saving to
// preferences works correctly. File System ID is used as a key in
// a base::DictionaryValue, so it has to be stored without path expansion.
const char kFileSystemId[] = "camera/pictures/id .!@#$%^&*()_+";

// Stores a provided file system information in preferences together with a
// fake observed entry.
void RememberFakeFileSystem(TestingProfile* profile,
                            const std::string& extension_id,
                            const std::string& file_system_id,
                            const std::string& display_name,
                            bool writable,
                            bool supports_notify_tag,
                            const ObservedEntry& observed_entry) {
  TestingPrefServiceSyncable* const pref_service =
      profile->GetTestingPrefService();
  ASSERT_TRUE(pref_service);

  base::DictionaryValue extensions;
  base::DictionaryValue* const file_systems = new base::DictionaryValue();
  base::DictionaryValue* const file_system = new base::DictionaryValue();
  file_system->SetStringWithoutPathExpansion(kPrefKeyFileSystemId,
                                             kFileSystemId);
  file_system->SetStringWithoutPathExpansion(kPrefKeyDisplayName, kDisplayName);
  file_system->SetBooleanWithoutPathExpansion(kPrefKeyWritable, writable);
  file_system->SetBooleanWithoutPathExpansion(kPrefKeySupportsNotifyTag,
                                              supports_notify_tag);
  file_systems->SetWithoutPathExpansion(kFileSystemId, file_system);
  extensions.SetWithoutPathExpansion(kExtensionId, file_systems);

  // Remember observed entries.
  base::DictionaryValue* const observed_entries = new base::DictionaryValue();
  file_system->SetWithoutPathExpansion(kPrefKeyObservedEntries,
                                       observed_entries);
  base::DictionaryValue* const observed_entry_value =
      new base::DictionaryValue();
  observed_entries->SetWithoutPathExpansion(observed_entry.entry_path.value(),
                                            observed_entry_value);
  observed_entry_value->SetStringWithoutPathExpansion(
      kPrefKeyObservedEntryEntryPath, observed_entry.entry_path.value());
  observed_entry_value->SetBooleanWithoutPathExpansion(
      kPrefKeyObservedEntryRecursive, observed_entry.recursive);
  observed_entry_value->SetStringWithoutPathExpansion(
      kPrefKeyObservedEntryLastTag, observed_entry.last_tag);

  pref_service->Set(prefs::kFileSystemProviderMounted, extensions);
}

}  // namespace

class FileSystemProviderRegistryTest : public testing::Test {
 protected:
  FileSystemProviderRegistryTest() : profile_(NULL) {}

  virtual ~FileSystemProviderRegistryTest() {}

  virtual void SetUp() override {
    profile_manager_.reset(
        new TestingProfileManager(TestingBrowserProcess::GetGlobal()));
    ASSERT_TRUE(profile_manager_->SetUp());
    profile_ = profile_manager_->CreateTestingProfile("test-user@example.com");
    registry_.reset(new Registry(profile_));
    fake_observed_entry_.entry_path =
        base::FilePath(FILE_PATH_LITERAL("/a/b/c"));
    fake_observed_entry_.recursive = true;
    fake_observed_entry_.last_tag = "hello-world";
  }

  content::TestBrowserThreadBundle thread_bundle_;
  scoped_ptr<TestingProfileManager> profile_manager_;
  TestingProfile* profile_;
  scoped_ptr<RegistryInterface> registry_;
  ObservedEntry fake_observed_entry_;
};

TEST_F(FileSystemProviderRegistryTest, RestoreFileSystems) {
  // Create a fake entry in the preferences.
  RememberFakeFileSystem(profile_,
                         kExtensionId,
                         kFileSystemId,
                         kDisplayName,
                         true /* writable */,
                         true /* supports_notify_tag */,
                         fake_observed_entry_);

  scoped_ptr<RegistryInterface::RestoredFileSystems> restored_file_systems =
      registry_->RestoreFileSystems(kExtensionId);

  ASSERT_EQ(1u, restored_file_systems->size());
  const RegistryInterface::RestoredFileSystem& restored_file_system =
      restored_file_systems->at(0);
  EXPECT_EQ(kExtensionId, restored_file_system.extension_id);
  EXPECT_EQ(kFileSystemId, restored_file_system.options.file_system_id);
  EXPECT_EQ(kDisplayName, restored_file_system.options.display_name);
  EXPECT_TRUE(restored_file_system.options.writable);
  EXPECT_TRUE(restored_file_system.options.supports_notify_tag);

  ASSERT_EQ(1u, restored_file_system.observed_entries.size());
  const auto& restored_observed_entry_it =
      restored_file_system.observed_entries.find(ObservedEntryKey(
          fake_observed_entry_.entry_path, fake_observed_entry_.recursive));
  ASSERT_NE(restored_file_system.observed_entries.end(),
            restored_observed_entry_it);

  EXPECT_EQ(fake_observed_entry_.entry_path,
            restored_observed_entry_it->second.entry_path);
  EXPECT_EQ(fake_observed_entry_.recursive,
            restored_observed_entry_it->second.recursive);
  EXPECT_EQ(fake_observed_entry_.last_tag,
            restored_observed_entry_it->second.last_tag);
}

TEST_F(FileSystemProviderRegistryTest, RememberFileSystem) {
  MountOptions options(kFileSystemId, kDisplayName);
  options.writable = true;
  options.supports_notify_tag = true;

  ProvidedFileSystemInfo file_system_info(
      kExtensionId, options, base::FilePath(FILE_PATH_LITERAL("/a/b/c")));

  ObservedEntries observed_entries;
  observed_entries[ObservedEntryKey(fake_observed_entry_.entry_path,
                                    fake_observed_entry_.recursive)] =
      fake_observed_entry_;

  registry_->RememberFileSystem(file_system_info, observed_entries);

  TestingPrefServiceSyncable* const pref_service =
      profile_->GetTestingPrefService();
  ASSERT_TRUE(pref_service);

  const base::DictionaryValue* const extensions =
      pref_service->GetDictionary(prefs::kFileSystemProviderMounted);
  ASSERT_TRUE(extensions);

  const base::DictionaryValue* file_systems = NULL;
  ASSERT_TRUE(extensions->GetDictionaryWithoutPathExpansion(kExtensionId,
                                                            &file_systems));
  EXPECT_EQ(1u, file_systems->size());

  const base::Value* file_system_value = NULL;
  const base::DictionaryValue* file_system = NULL;
  ASSERT_TRUE(
      file_systems->GetWithoutPathExpansion(kFileSystemId, &file_system_value));
  ASSERT_TRUE(file_system_value->GetAsDictionary(&file_system));

  std::string file_system_id;
  EXPECT_TRUE(file_system->GetStringWithoutPathExpansion(kPrefKeyFileSystemId,
                                                         &file_system_id));
  EXPECT_EQ(kFileSystemId, file_system_id);

  std::string display_name;
  EXPECT_TRUE(file_system->GetStringWithoutPathExpansion(kPrefKeyDisplayName,
                                                         &display_name));
  EXPECT_EQ(kDisplayName, display_name);

  bool writable = false;
  EXPECT_TRUE(
      file_system->GetBooleanWithoutPathExpansion(kPrefKeyWritable, &writable));
  EXPECT_TRUE(writable);

  bool supports_notify_tag = false;
  EXPECT_TRUE(file_system->GetBooleanWithoutPathExpansion(
      kPrefKeySupportsNotifyTag, &supports_notify_tag));
  EXPECT_TRUE(supports_notify_tag);

  const base::DictionaryValue* observed_entries_value = NULL;
  ASSERT_TRUE(file_system->GetDictionaryWithoutPathExpansion(
      kPrefKeyObservedEntries, &observed_entries_value));

  const base::DictionaryValue* observed_entry = NULL;
  ASSERT_TRUE(observed_entries_value->GetDictionaryWithoutPathExpansion(
      fake_observed_entry_.entry_path.value(), &observed_entry));

  std::string entry_path;
  EXPECT_TRUE(observed_entry->GetStringWithoutPathExpansion(
      kPrefKeyObservedEntryEntryPath, &entry_path));
  EXPECT_EQ(fake_observed_entry_.entry_path.value(), entry_path);

  bool recursive = false;
  EXPECT_TRUE(observed_entry->GetBooleanWithoutPathExpansion(
      kPrefKeyObservedEntryRecursive, &recursive));
  EXPECT_EQ(fake_observed_entry_.recursive, recursive);

  std::string last_tag;
  EXPECT_TRUE(observed_entry->GetStringWithoutPathExpansion(
      kPrefKeyObservedEntryLastTag, &last_tag));
  EXPECT_EQ(fake_observed_entry_.last_tag, last_tag);
}

TEST_F(FileSystemProviderRegistryTest, ForgetFileSystem) {
  // Create a fake file systems in the preferences.
  RememberFakeFileSystem(profile_,
                         kExtensionId,
                         kFileSystemId,
                         kDisplayName,
                         true /* writable */,
                         true /* supports_notify_tag */,
                         fake_observed_entry_);

  registry_->ForgetFileSystem(kExtensionId, kFileSystemId);

  TestingPrefServiceSyncable* const pref_service =
      profile_->GetTestingPrefService();
  ASSERT_TRUE(pref_service);

  const base::DictionaryValue* const extensions =
      pref_service->GetDictionary(prefs::kFileSystemProviderMounted);
  ASSERT_TRUE(extensions);

  const base::DictionaryValue* file_systems = NULL;
  EXPECT_FALSE(extensions->GetDictionaryWithoutPathExpansion(kExtensionId,
                                                             &file_systems));
}

TEST_F(FileSystemProviderRegistryTest, UpdateObservedEntryTag) {
  MountOptions options(kFileSystemId, kDisplayName);
  options.writable = true;
  options.supports_notify_tag = true;

  ProvidedFileSystemInfo file_system_info(
      kExtensionId, options, base::FilePath(FILE_PATH_LITERAL("/a/b/c")));

  ObservedEntries observed_entries;
  observed_entries[ObservedEntryKey(fake_observed_entry_.entry_path,
                                    fake_observed_entry_.recursive)] =
      fake_observed_entry_;

  registry_->RememberFileSystem(file_system_info, observed_entries);

  fake_observed_entry_.last_tag = "updated-tag";
  registry_->UpdateObservedEntryTag(file_system_info, fake_observed_entry_);

  TestingPrefServiceSyncable* const pref_service =
      profile_->GetTestingPrefService();
  ASSERT_TRUE(pref_service);

  const base::DictionaryValue* const extensions =
      pref_service->GetDictionary(prefs::kFileSystemProviderMounted);
  ASSERT_TRUE(extensions);

  const base::DictionaryValue* file_systems = NULL;
  ASSERT_TRUE(extensions->GetDictionaryWithoutPathExpansion(kExtensionId,
                                                            &file_systems));
  EXPECT_EQ(1u, file_systems->size());

  const base::Value* file_system_value = NULL;
  const base::DictionaryValue* file_system = NULL;
  ASSERT_TRUE(
      file_systems->GetWithoutPathExpansion(kFileSystemId, &file_system_value));
  ASSERT_TRUE(file_system_value->GetAsDictionary(&file_system));

  std::string file_system_id;
  EXPECT_TRUE(file_system->GetStringWithoutPathExpansion(kPrefKeyFileSystemId,
                                                         &file_system_id));
  EXPECT_EQ(kFileSystemId, file_system_id);

  std::string display_name;
  EXPECT_TRUE(file_system->GetStringWithoutPathExpansion(kPrefKeyDisplayName,
                                                         &display_name));
  EXPECT_EQ(kDisplayName, display_name);

  bool writable = false;
  EXPECT_TRUE(
      file_system->GetBooleanWithoutPathExpansion(kPrefKeyWritable, &writable));
  EXPECT_TRUE(writable);

  bool supports_notify_tag = false;
  EXPECT_TRUE(file_system->GetBooleanWithoutPathExpansion(
      kPrefKeySupportsNotifyTag, &supports_notify_tag));
  EXPECT_TRUE(supports_notify_tag);

  const base::DictionaryValue* observed_entries_value = NULL;
  ASSERT_TRUE(file_system->GetDictionaryWithoutPathExpansion(
      kPrefKeyObservedEntries, &observed_entries_value));

  const base::DictionaryValue* observed_entry = NULL;
  ASSERT_TRUE(observed_entries_value->GetDictionaryWithoutPathExpansion(
      fake_observed_entry_.entry_path.value(), &observed_entry));

  std::string entry_path;
  EXPECT_TRUE(observed_entry->GetStringWithoutPathExpansion(
      kPrefKeyObservedEntryEntryPath, &entry_path));
  EXPECT_EQ(fake_observed_entry_.entry_path.value(), entry_path);

  bool recursive = false;
  EXPECT_TRUE(observed_entry->GetBooleanWithoutPathExpansion(
      kPrefKeyObservedEntryRecursive, &recursive));
  EXPECT_EQ(fake_observed_entry_.recursive, recursive);

  std::string last_tag;
  EXPECT_TRUE(observed_entry->GetStringWithoutPathExpansion(
      kPrefKeyObservedEntryLastTag, &last_tag));
  EXPECT_EQ(fake_observed_entry_.last_tag, last_tag);
}

}  // namespace file_system_provider
}  // namespace chromeos
