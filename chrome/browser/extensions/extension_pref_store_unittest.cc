// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/browser/extensions/extension_pref_store.h"
#include "chrome/browser/pref_service.h"
#include "chrome/browser/pref_value_store.h"

namespace {

class TestExtensionPrefStore : public ExtensionPrefStore {
 public:
  TestExtensionPrefStore() : ExtensionPrefStore(NULL) {}

  typedef std::vector<std::string> ExtensionIDs;
  void GetExtensionIDList(ExtensionIDs* result) {
    GetExtensionIDs(result);
  }
};

class MockPrefService : public PrefService {
 public:
  explicit MockPrefService(PrefValueStore* value_store)
      : PrefService(value_store) {}

  // Tracks whether the observers would have been notified.
  virtual void FireObserversIfChanged(const wchar_t* pref_name,
                                      const Value* old_value) {
    fired_observers_ = PrefIsChanged(pref_name, old_value);
  }

  bool fired_observers_;
};

// Use static constants to avoid confusing std::map with hard-coded strings.
static const wchar_t* kPref1 = L"path1.subpath";
static const wchar_t* kPref2 = L"path2";
static const wchar_t* kPref3 = L"path3";
static const wchar_t* kPref4 = L"path4";

}  // namespace

TEST(ExtensionPrefStoreTest, InstallOneExtension) {
  TestExtensionPrefStore eps;
  eps.InstallExtensionPref("id1", kPref1, Value::CreateStringValue("val1"));

  TestExtensionPrefStore::ExtensionIDs ids;
  eps.GetExtensionIDList(&ids);
  EXPECT_EQ(1u, ids.size());
  EXPECT_EQ("id1", ids[0]);

  DictionaryValue* prefs = eps.prefs();
  ASSERT_EQ(1u, prefs->size());
  std::string actual;
  ASSERT_TRUE(prefs->GetString(kPref1, &actual));
  EXPECT_EQ("val1", actual);
}

// Make sure the last-installed extension wins.
TEST(ExtensionPrefStoreTest, InstallMultipleExtensions) {
  TestExtensionPrefStore eps;
  eps.InstallExtensionPref("id1", kPref1, Value::CreateStringValue("val1"));
  eps.InstallExtensionPref("id2", kPref1, Value::CreateStringValue("val2"));
  eps.InstallExtensionPref("id3", kPref1, Value::CreateStringValue("val3"));

  TestExtensionPrefStore::ExtensionIDs ids;
  eps.GetExtensionIDList(&ids);
  EXPECT_EQ(3u, ids.size());
  EXPECT_EQ("id3", ids[0]);
  EXPECT_EQ("id2", ids[1]);
  EXPECT_EQ("id1", ids[2]);

  DictionaryValue* prefs = eps.prefs();
  ASSERT_EQ(1u, prefs->size());
  std::string actual;
  ASSERT_TRUE(prefs->GetString(kPref1, &actual));
  EXPECT_EQ("val3", actual);
}

// Make sure the last-installed extension wins for each preference.
TEST(ExtensionPrefStoreTest, InstallOverwrittenExtensions) {
  TestExtensionPrefStore eps;
  eps.InstallExtensionPref("id1", kPref1, Value::CreateStringValue("val1"));
  eps.InstallExtensionPref("id2", kPref1, Value::CreateStringValue("val2"));
  eps.InstallExtensionPref("id3", kPref1, Value::CreateStringValue("val3"));

  eps.InstallExtensionPref("id1", kPref2, Value::CreateStringValue("val4"));
  eps.InstallExtensionPref("id2", kPref2, Value::CreateStringValue("val5"));

  eps.InstallExtensionPref("id1", kPref1, Value::CreateStringValue("val6"));
  eps.InstallExtensionPref("id1", kPref2, Value::CreateStringValue("val7"));
  eps.InstallExtensionPref("id1", kPref3, Value::CreateStringValue("val8"));

  TestExtensionPrefStore::ExtensionIDs ids;
  eps.GetExtensionIDList(&ids);
  EXPECT_EQ(3u, ids.size());
  EXPECT_EQ("id3", ids[0]);
  EXPECT_EQ("id2", ids[1]);
  EXPECT_EQ("id1", ids[2]);

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
  eps.InstallExtensionPref("id1", kPref1, Value::CreateStringValue("val1"));
  eps.InstallExtensionPref("id2", kPref2, Value::CreateStringValue("val2"));
  eps.InstallExtensionPref("id3", kPref3, Value::CreateStringValue("val3"));

  eps.InstallExtensionPref("id3", kPref3, Value::CreateStringValue("val4"));
  eps.InstallExtensionPref("id2", kPref3, Value::CreateStringValue("val5"));
  eps.InstallExtensionPref("id1", kPref3, Value::CreateStringValue("val6"));

  eps.InstallExtensionPref("id3", kPref1, Value::CreateStringValue("val7"));

  TestExtensionPrefStore::ExtensionIDs ids;
  eps.GetExtensionIDList(&ids);
  EXPECT_EQ(3u, ids.size());
  EXPECT_EQ("id3", ids[0]);
  EXPECT_EQ("id2", ids[1]);
  EXPECT_EQ("id1", ids[2]);

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
  eps.InstallExtensionPref("id1", kPref1, Value::CreateStringValue("val1"));
  eps.InstallExtensionPref("id1", kPref2, Value::CreateStringValue("val2"));

  // No need to check the state here; the InstallOneExtension already has.
  eps.UninstallExtension("id1");

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
  eps.InstallExtensionPref("id1", kPref1, Value::CreateStringValue("val1"));
  eps.InstallExtensionPref("id2", kPref1, Value::CreateStringValue("val2"));

  eps.InstallExtensionPref("id1", kPref2, Value::CreateStringValue("val3"));
  eps.InstallExtensionPref("id2", kPref2, Value::CreateStringValue("val4"));

  eps.UninstallExtension("id1");

  TestExtensionPrefStore::ExtensionIDs ids;
  eps.GetExtensionIDList(&ids);
  EXPECT_EQ(1u, ids.size());
  EXPECT_EQ("id2", ids[0]);

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
  eps.InstallExtensionPref("id1", kPref1, Value::CreateStringValue("val1"));
  eps.InstallExtensionPref("id2", kPref1, Value::CreateStringValue("val2"));
  eps.InstallExtensionPref("id3", kPref1, Value::CreateStringValue("val3"));

  eps.InstallExtensionPref("id1", kPref2, Value::CreateStringValue("val4"));
  eps.InstallExtensionPref("id3", kPref2, Value::CreateStringValue("val5"));

  eps.UninstallExtension("id3");

  TestExtensionPrefStore::ExtensionIDs ids;
  eps.GetExtensionIDList(&ids);
  EXPECT_EQ(2u, ids.size());
  EXPECT_EQ("id2", ids[0]);
  EXPECT_EQ("id1", ids[1]);

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
  eps.InstallExtensionPref("id1", kPref1, Value::CreateStringValue("val1"));
  eps.InstallExtensionPref("id2", kPref1, Value::CreateStringValue("val2"));
  eps.InstallExtensionPref("id3", kPref1, Value::CreateStringValue("val3"));

  eps.InstallExtensionPref("id1", kPref2, Value::CreateStringValue("val4"));
  eps.InstallExtensionPref("id2", kPref2, Value::CreateStringValue("val5"));

  eps.InstallExtensionPref("id1", kPref3, Value::CreateStringValue("val6"));

  eps.InstallExtensionPref("id2", kPref4, Value::CreateStringValue("val7"));

  eps.UninstallExtension("id2");

  TestExtensionPrefStore::ExtensionIDs ids;
  eps.GetExtensionIDList(&ids);
  EXPECT_EQ(2u, ids.size());
  EXPECT_EQ("id3", ids[0]);
  EXPECT_EQ("id1", ids[1]);

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
  TestExtensionPrefStore eps;

  // The PrefValueStore takes ownership of the PrefStores; in this case, that's
  // only an ExtensionPrefStore.
  PrefValueStore* value_store = new PrefValueStore(NULL, &eps, NULL, NULL,
      NULL);
  MockPrefService* pref_service = new MockPrefService(value_store);
  eps.SetPrefService(pref_service);
  pref_service->RegisterStringPref(kPref1, std::string());

  eps.InstallExtensionPref("abc", kPref1,
      Value::CreateStringValue("https://www.chromium.org"));
  EXPECT_TRUE(pref_service->fired_observers_);
  eps.InstallExtensionPref("abc", kPref1,
      Value::CreateStringValue("https://www.chromium.org"));
  EXPECT_FALSE(pref_service->fired_observers_);
  eps.InstallExtensionPref("abc", kPref1,
      Value::CreateStringValue("chrome://newtab"));
  EXPECT_TRUE(pref_service->fired_observers_);

  eps.UninstallExtension("abc");
  EXPECT_TRUE(pref_service->fired_observers_);
}
