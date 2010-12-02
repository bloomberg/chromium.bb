// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/scoped_ptr.h"
#include "base/scoped_temp_dir.h"
#include "base/values.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/browser/prefs/default_pref_store.h"
#include "chrome/browser/prefs/pref_service.h"
#include "chrome/browser/prefs/pref_value_store.h"
#include "chrome/common/extensions/extension.h"
#include "chrome/test/testing_pref_service.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace keys = extension_manifest_keys;

namespace {

class TestExtensionPrefStore : public ExtensionPrefStore {
 public:
  TestExtensionPrefStore()
      : ExtensionPrefStore(NULL, PrefNotifier::EXTENSION_STORE),
        ext1(NULL),
        ext2(NULL),
        ext3(NULL),
        pref_service_(NULL) {
    // Can't use ASSERT_TRUE here because a constructor can't return a value.
    if (!temp_dir_.CreateUniqueTempDir()) {
      ADD_FAILURE() << "Failed to create temp dir";
      return;
    }
    DictionaryValue simple_dict;
    std::string error;

    simple_dict.SetString(keys::kVersion, "1.0.0.0");
    simple_dict.SetString(keys::kName, "unused");

    ext1_scoped_ = Extension::Create(
        temp_dir_.path().AppendASCII("ext1"), Extension::INVALID,
        simple_dict, false, &error);
    ext2_scoped_ = Extension::Create(
        temp_dir_.path().AppendASCII("ext2"), Extension::INVALID,
        simple_dict, false, &error);
    ext3_scoped_ = Extension::Create(
        temp_dir_.path().AppendASCII("ext3"), Extension::INVALID,
        simple_dict, false, &error);

    ext1 = ext1_scoped_.get();
    ext2 = ext2_scoped_.get();
    ext3 = ext3_scoped_.get();
  }

  typedef std::vector<std::string> ExtensionIDs;
  void GetExtensionIDList(ExtensionIDs* result) {
    GetExtensionIDs(result);
  }

  void SetPrefService(PrefService* pref_service) {
    pref_service_ = pref_service;
  }

  // Overridden from ExtensionPrefStore.
  virtual PrefService* GetPrefService() {
    return pref_service_;
  }

  // Weak references, for convenience.
  Extension* ext1;
  Extension* ext2;
  Extension* ext3;

 private:
  ScopedTempDir temp_dir_;

  scoped_refptr<Extension> ext1_scoped_;
  scoped_refptr<Extension> ext2_scoped_;
  scoped_refptr<Extension> ext3_scoped_;

  // Weak reference.
  PrefService* pref_service_;
};

// Mock PrefNotifier that allows the notifications to be tracked.
class MockPrefNotifier : public PrefNotifier {
 public:
  MockPrefNotifier(PrefService* service, PrefValueStore* value_store)
    : PrefNotifier(service, value_store) {}

  virtual ~MockPrefNotifier() {}

  MOCK_METHOD1(FireObservers, void(const char* path));
};

// Mock PrefService that allows the PrefNotifier to be injected.
class MockPrefService : public PrefService {
 public:
  explicit MockPrefService(PrefValueStore* pref_value_store)
      : PrefService(pref_value_store) {
  }

  void SetPrefNotifier(MockPrefNotifier* notifier) {
    pref_notifier_.reset(notifier);
  }
};

// Use constants to avoid confusing std::map with hard-coded strings.
const char kPref1[] = "path1.subpath";
const char kPref2[] = "path2";
const char kPref3[] = "path3";
const char kPref4[] = "path4";

}  // namespace

TEST(ExtensionPrefStoreTest, InstallOneExtension) {
  TestExtensionPrefStore eps;
  ASSERT_TRUE(eps.ext1 != NULL);
  eps.InstallExtensionPref(eps.ext1, kPref1, Value::CreateStringValue("val1"));

  TestExtensionPrefStore::ExtensionIDs ids;
  eps.GetExtensionIDList(&ids);
  EXPECT_EQ(1u, ids.size());
  EXPECT_EQ(eps.ext1->id(), ids[0]);

  DictionaryValue* prefs = eps.prefs();
  ASSERT_EQ(1u, prefs->size());
  std::string actual;
  ASSERT_TRUE(prefs->GetString(kPref1, &actual));
  EXPECT_EQ("val1", actual);
}

// Make sure the last-installed extension wins.
TEST(ExtensionPrefStoreTest, InstallMultipleExtensions) {
  TestExtensionPrefStore eps;
  ASSERT_TRUE(eps.ext1 != NULL);
  eps.InstallExtensionPref(eps.ext1, kPref1, Value::CreateStringValue("val1"));
  eps.InstallExtensionPref(eps.ext2, kPref1, Value::CreateStringValue("val2"));
  eps.InstallExtensionPref(eps.ext3, kPref1, Value::CreateStringValue("val3"));

  TestExtensionPrefStore::ExtensionIDs ids;
  eps.GetExtensionIDList(&ids);
  EXPECT_EQ(3u, ids.size());
  EXPECT_EQ(eps.ext3->id(), ids[0]);
  EXPECT_EQ(eps.ext2->id(), ids[1]);
  EXPECT_EQ(eps.ext1->id(), ids[2]);

  DictionaryValue* prefs = eps.prefs();
  ASSERT_EQ(1u, prefs->size());
  std::string actual;
  ASSERT_TRUE(prefs->GetString(kPref1, &actual));
  EXPECT_EQ("val3", actual);
}

// Make sure the last-installed extension wins for each preference.
TEST(ExtensionPrefStoreTest, InstallOverwrittenExtensions) {
  TestExtensionPrefStore eps;
  ASSERT_TRUE(eps.ext1 != NULL);
  eps.InstallExtensionPref(eps.ext1, kPref1, Value::CreateStringValue("val1"));
  eps.InstallExtensionPref(eps.ext2, kPref1, Value::CreateStringValue("val2"));
  eps.InstallExtensionPref(eps.ext3, kPref1, Value::CreateStringValue("val3"));

  eps.InstallExtensionPref(eps.ext1, kPref2, Value::CreateStringValue("val4"));
  eps.InstallExtensionPref(eps.ext2, kPref2, Value::CreateStringValue("val5"));

  eps.InstallExtensionPref(eps.ext1, kPref1, Value::CreateStringValue("val6"));
  eps.InstallExtensionPref(eps.ext1, kPref2, Value::CreateStringValue("val7"));
  eps.InstallExtensionPref(eps.ext1, kPref3, Value::CreateStringValue("val8"));

  TestExtensionPrefStore::ExtensionIDs ids;
  eps.GetExtensionIDList(&ids);
  EXPECT_EQ(3u, ids.size());
  EXPECT_EQ(eps.ext3->id(), ids[0]);
  EXPECT_EQ(eps.ext2->id(), ids[1]);
  EXPECT_EQ(eps.ext1->id(), ids[2]);

  DictionaryValue* prefs = eps.prefs();
  ASSERT_EQ(3u, prefs->size());
  std::string actual;
  EXPECT_TRUE(prefs->GetString(kPref1, &actual));
  EXPECT_EQ("val3", actual);
  EXPECT_TRUE(prefs->GetString(kPref2, &actual));
  EXPECT_EQ("val5", actual);
  EXPECT_TRUE(prefs->GetString(kPref3, &actual));
  EXPECT_EQ("val8", actual);
}

// Make sure the last-installed extension wins even if other extensions set
// the same or different preferences later.
TEST(ExtensionPrefStoreTest, InstallInterleavedExtensions) {
  TestExtensionPrefStore eps;
  ASSERT_TRUE(eps.ext1 != NULL);
  eps.InstallExtensionPref(eps.ext1, kPref1, Value::CreateStringValue("val1"));
  eps.InstallExtensionPref(eps.ext2, kPref2, Value::CreateStringValue("val2"));
  eps.InstallExtensionPref(eps.ext3, kPref3, Value::CreateStringValue("val3"));

  eps.InstallExtensionPref(eps.ext3, kPref3, Value::CreateStringValue("val4"));
  eps.InstallExtensionPref(eps.ext2, kPref3, Value::CreateStringValue("val5"));
  eps.InstallExtensionPref(eps.ext1, kPref3, Value::CreateStringValue("val6"));

  eps.InstallExtensionPref(eps.ext3, kPref1, Value::CreateStringValue("val7"));

  TestExtensionPrefStore::ExtensionIDs ids;
  eps.GetExtensionIDList(&ids);
  EXPECT_EQ(3u, ids.size());
  EXPECT_EQ(eps.ext3->id(), ids[0]);
  EXPECT_EQ(eps.ext2->id(), ids[1]);
  EXPECT_EQ(eps.ext1->id(), ids[2]);

  DictionaryValue* prefs = eps.prefs();
  ASSERT_EQ(3u, prefs->size());
  std::string actual;
  EXPECT_TRUE(prefs->GetString(kPref1, &actual));
  EXPECT_EQ("val7", actual);
  EXPECT_TRUE(prefs->GetString(kPref2, &actual));
  EXPECT_EQ("val2", actual);
  EXPECT_TRUE(prefs->GetString(kPref3, &actual));
  EXPECT_EQ("val4", actual);
}

TEST(ExtensionPrefStoreTest, UninstallOnlyExtension) {
  TestExtensionPrefStore eps;
  ASSERT_TRUE(eps.ext1 != NULL);
  eps.InstallExtensionPref(eps.ext1, kPref1, Value::CreateStringValue("val1"));
  eps.InstallExtensionPref(eps.ext1, kPref2, Value::CreateStringValue("val2"));

  // No need to check the state here; the Install* tests cover that.
  eps.UninstallExtension(eps.ext1);

  TestExtensionPrefStore::ExtensionIDs ids;
  eps.GetExtensionIDList(&ids);
  EXPECT_EQ(0u, ids.size());

  DictionaryValue* prefs = eps.prefs();
  std::string actual;
  // "path1.name" has been removed, but an empty "path1" dictionary is still
  // present.
  ASSERT_EQ(1u, prefs->size());
  EXPECT_FALSE(prefs->GetString(kPref1, &actual));
  EXPECT_FALSE(prefs->GetString(kPref2, &actual));
}

// Tests uninstalling an extension that wasn't winning for any preferences.
TEST(ExtensionPrefStoreTest, UninstallIrrelevantExtension) {
  TestExtensionPrefStore eps;
  ASSERT_TRUE(eps.ext1 != NULL);
  eps.InstallExtensionPref(eps.ext1, kPref1, Value::CreateStringValue("val1"));
  eps.InstallExtensionPref(eps.ext2, kPref1, Value::CreateStringValue("val2"));

  eps.InstallExtensionPref(eps.ext1, kPref2, Value::CreateStringValue("val3"));
  eps.InstallExtensionPref(eps.ext2, kPref2, Value::CreateStringValue("val4"));

  eps.UninstallExtension(eps.ext1);

  TestExtensionPrefStore::ExtensionIDs ids;
  eps.GetExtensionIDList(&ids);
  EXPECT_EQ(1u, ids.size());
  EXPECT_EQ(eps.ext2->id(), ids[0]);

  DictionaryValue* prefs = eps.prefs();
  ASSERT_EQ(2u, prefs->size());
  std::string actual;
  EXPECT_TRUE(prefs->GetString(kPref1, &actual));
  EXPECT_EQ("val2", actual);
  EXPECT_TRUE(prefs->GetString(kPref2, &actual));
  EXPECT_EQ("val4", actual);
}

// Tests uninstalling an extension that was winning for all preferences.
TEST(ExtensionPrefStoreTest, UninstallExtensionFromTop) {
  TestExtensionPrefStore eps;
  ASSERT_TRUE(eps.ext1 != NULL);
  eps.InstallExtensionPref(eps.ext1, kPref1, Value::CreateStringValue("val1"));
  eps.InstallExtensionPref(eps.ext2, kPref1, Value::CreateStringValue("val2"));
  eps.InstallExtensionPref(eps.ext3, kPref1, Value::CreateStringValue("val3"));

  eps.InstallExtensionPref(eps.ext1, kPref2, Value::CreateStringValue("val4"));
  eps.InstallExtensionPref(eps.ext3, kPref2, Value::CreateStringValue("val5"));

  eps.UninstallExtension(eps.ext3);

  TestExtensionPrefStore::ExtensionIDs ids;
  eps.GetExtensionIDList(&ids);
  EXPECT_EQ(2u, ids.size());
  EXPECT_EQ(eps.ext2->id(), ids[0]);
  EXPECT_EQ(eps.ext1->id(), ids[1]);

  DictionaryValue* prefs = eps.prefs();
  ASSERT_EQ(2u, prefs->size());
  std::string actual;
  EXPECT_TRUE(prefs->GetString(kPref1, &actual));
  EXPECT_EQ("val2", actual);
  EXPECT_TRUE(prefs->GetString(kPref2, &actual));
  EXPECT_EQ("val4", actual);
}

// Tests uninstalling an extension that was winning for only some preferences.
TEST(ExtensionPrefStoreTest, UninstallExtensionFromMiddle) {
  TestExtensionPrefStore eps;
  ASSERT_TRUE(eps.ext1 != NULL);
  eps.InstallExtensionPref(eps.ext1, kPref1, Value::CreateStringValue("val1"));
  eps.InstallExtensionPref(eps.ext2, kPref1, Value::CreateStringValue("val2"));
  eps.InstallExtensionPref(eps.ext3, kPref1, Value::CreateStringValue("val3"));

  eps.InstallExtensionPref(eps.ext1, kPref2, Value::CreateStringValue("val4"));
  eps.InstallExtensionPref(eps.ext2, kPref2, Value::CreateStringValue("val5"));

  eps.InstallExtensionPref(eps.ext1, kPref3, Value::CreateStringValue("val6"));

  eps.InstallExtensionPref(eps.ext2, kPref4, Value::CreateStringValue("val7"));

  eps.UninstallExtension(eps.ext2);

  TestExtensionPrefStore::ExtensionIDs ids;
  eps.GetExtensionIDList(&ids);
  EXPECT_EQ(2u, ids.size());
  EXPECT_EQ(eps.ext3->id(), ids[0]);
  EXPECT_EQ(eps.ext1->id(), ids[1]);

  DictionaryValue* prefs = eps.prefs();
  ASSERT_EQ(3u, prefs->size());
  std::string actual;
  EXPECT_TRUE(prefs->GetString(kPref1, &actual));
  EXPECT_EQ("val3", actual);
  EXPECT_TRUE(prefs->GetString(kPref2, &actual));
  EXPECT_EQ("val4", actual);
  EXPECT_TRUE(prefs->GetString(kPref3, &actual));
  EXPECT_EQ("val6", actual);
  EXPECT_FALSE(prefs->GetString(kPref4, &actual));
}

TEST(ExtensionPrefStoreTest, NotifyWhenNeeded) {
  using testing::Mock;

  TestExtensionPrefStore* eps = new TestExtensionPrefStore;
  DefaultPrefStore* dps = new DefaultPrefStore;
  ASSERT_TRUE(eps->ext1 != NULL);

  // The PrefValueStore takes ownership of the PrefStores; in this case, that's
  // only an ExtensionPrefStore. Likewise, the PrefService takes ownership of
  // the PrefValueStore and PrefNotifier.
  PrefValueStore* value_store = new TestingPrefService::TestingPrefValueStore(
      NULL, NULL, eps, NULL, NULL, NULL, dps);
  scoped_ptr<MockPrefService> pref_service(new MockPrefService(value_store));
  MockPrefNotifier* pref_notifier = new MockPrefNotifier(pref_service.get(),
      value_store);
  pref_service->SetPrefNotifier(pref_notifier);

  eps->SetPrefService(pref_service.get());
  pref_service->RegisterStringPref(kPref1, std::string());

  EXPECT_CALL(*pref_notifier, FireObservers(kPref1));
  eps->InstallExtensionPref(eps->ext1, kPref1,
      Value::CreateStringValue("https://www.chromium.org"));
  Mock::VerifyAndClearExpectations(pref_notifier);

  EXPECT_CALL(*pref_notifier, FireObservers(kPref1)).Times(0);
  eps->InstallExtensionPref(eps->ext1, kPref1,
      Value::CreateStringValue("https://www.chromium.org"));
  Mock::VerifyAndClearExpectations(pref_notifier);

  EXPECT_CALL(*pref_notifier, FireObservers(kPref1)).Times(2);
  eps->InstallExtensionPref(eps->ext1, kPref1,
      Value::CreateStringValue("chrome://newtab"));
  eps->UninstallExtension(eps->ext1);
}
