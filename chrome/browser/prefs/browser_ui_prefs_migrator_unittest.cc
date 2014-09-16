// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/observer_list.h"
#include "base/prefs/writeable_pref_store.h"
#include "base/values.h"
#include "chrome/browser/devtools/devtools_window.h"
#include "chrome/browser/prefs/browser_ui_prefs_migrator.h"
#include "chrome/common/pref_names.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class DictionaryPrefStore : public WriteablePrefStore {
 public:
  DictionaryPrefStore() : WriteablePrefStore() {}

  // Overrides from PrefStore.
  virtual void AddObserver(Observer* observer) OVERRIDE {
    observers_.AddObserver(observer);
  }

  virtual void RemoveObserver(Observer* observer) OVERRIDE {
    observers_.RemoveObserver(observer);
  }

  virtual bool GetValue(const std::string& key,
                        const base::Value** result) const OVERRIDE {
    return prefs_.Get(key, result);
  }

  // Overrides from WriteablePrefStore.
  virtual void SetValue(const std::string& key, base::Value* value) OVERRIDE {
    DCHECK(value);
    prefs_.Set(key, value);
    ReportValueChanged(key);
  }

  virtual void RemoveValue(const std::string& key) OVERRIDE {
    if (prefs_.RemovePath(key, NULL))
      ReportValueChanged(key);
  }

  virtual bool GetMutableValue(const std::string& key,
                               base::Value** result) OVERRIDE {
    return prefs_.Get(key, result);
  }

  virtual void ReportValueChanged(const std::string& key) OVERRIDE {}

  virtual void SetValueSilently(const std::string& key,
                                base::Value* value) OVERRIDE {
    NOTIMPLEMENTED();
  }

  void SignalObservers() {
    FOR_EACH_OBSERVER(
        PrefStore::Observer, observers_, OnInitializationCompleted(true));
  }

 private:
  virtual ~DictionaryPrefStore() {}

  base::DictionaryValue prefs_;
  ObserverList<PrefStore::Observer, true> observers_;

  DISALLOW_COPY_AND_ASSIGN(DictionaryPrefStore);
};

}  // namespace

TEST(UIPrefsMigratorTest, MigrateTest) {
  scoped_refptr<DictionaryPrefStore> pref_store(new DictionaryPrefStore);
  scoped_ptr<base::DictionaryValue> browser_window_placement(
      new base::DictionaryValue);
  browser_window_placement->SetInteger("bottom", 1000);
  pref_store->SetValue(prefs::kBrowserWindowPlacement,
                       browser_window_placement.release());

  scoped_ptr<base::DictionaryValue> browser_window_placement_popup(
      new base::DictionaryValue);
  browser_window_placement_popup->SetInteger("top", 50);
  pref_store->SetValue(prefs::kBrowserWindowPlacementPopup,
                       browser_window_placement_popup.release());

  scoped_ptr<base::DictionaryValue> single_app_placement_dict(
      new base::DictionaryValue);
  single_app_placement_dict->SetInteger("right", 986);
  const char* kAppName("localhost_/some_app.html");
  const std::string kOldPathToOneAppDictionary =
      prefs::kBrowserWindowPlacement + std::string("_") + kAppName;
  pref_store->SetValue(kOldPathToOneAppDictionary,
                       single_app_placement_dict.release());
  EXPECT_TRUE(pref_store->GetValue(kOldPathToOneAppDictionary, NULL));

  scoped_ptr<base::DictionaryValue> devtools_placement_dict(
      new base::DictionaryValue);
  devtools_placement_dict->SetInteger("left", 700);
  const std::string kOldPathToDevToolsDictionary =
      prefs::kBrowserWindowPlacement + std::string("_") +
      DevToolsWindow::kDevToolsApp;
  pref_store->SetValue(kOldPathToDevToolsDictionary,
                       devtools_placement_dict.release());
  EXPECT_TRUE(pref_store->GetValue(kOldPathToDevToolsDictionary, NULL));

  pref_store->AddObserver(new BrowserUIPrefsMigrator(pref_store.get()));
  pref_store->SignalObservers();

  EXPECT_FALSE(pref_store->GetValue(kOldPathToOneAppDictionary, NULL));
  EXPECT_FALSE(pref_store->GetValue(kOldPathToDevToolsDictionary, NULL));

  const base::Value* value = NULL;
  const base::DictionaryValue* dictionary = NULL;
  int out_value;

  ASSERT_TRUE(pref_store->GetValue(prefs::kBrowserWindowPlacement, &value));
  ASSERT_TRUE(value->GetAsDictionary(&dictionary));
  EXPECT_TRUE(dictionary->GetInteger("bottom", &out_value));
  EXPECT_EQ(1000, out_value);

  ASSERT_TRUE(
      pref_store->GetValue(prefs::kBrowserWindowPlacementPopup, &value));
  ASSERT_TRUE(value->GetAsDictionary(&dictionary));
  EXPECT_TRUE(dictionary->GetInteger("top", &out_value));
  EXPECT_EQ(50, out_value);

  ASSERT_TRUE(pref_store->GetValue(
      prefs::kAppWindowPlacement + std::string(".") + kAppName, &value));
  ASSERT_TRUE(value->GetAsDictionary(&dictionary));
  EXPECT_TRUE(dictionary->GetInteger("right", &out_value));
  EXPECT_EQ(986, out_value);

  ASSERT_TRUE(pref_store->GetValue(prefs::kAppWindowPlacement +
                                       std::string(".") +
                                       DevToolsWindow::kDevToolsApp,
                                   &value));
  ASSERT_TRUE(value->GetAsDictionary(&dictionary));
  EXPECT_TRUE(dictionary->GetInteger("left", &out_value));
  EXPECT_EQ(700, out_value);
}
