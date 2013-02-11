// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/browsing_data/browsing_data_api.h"

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_service.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/browsing_data/browsing_data_helper.h"
#include "chrome/browser/browsing_data/browsing_data_remover.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/notification_service.h"

using extension_function_test_utils::RunFunctionAndReturnError;
using extension_function_test_utils::RunFunctionAndReturnSingleResult;

namespace {

enum OriginSetMask {
  UNPROTECTED_WEB = BrowsingDataHelper::UNPROTECTED_WEB,
  PROTECTED_WEB = BrowsingDataHelper::PROTECTED_WEB,
  EXTENSION = BrowsingDataHelper::EXTENSION
};

const char kRemoveEverythingArguments[] = "[{\"since\": 1000}, {"
    "\"appcache\": true, \"cache\": true, \"cookies\": true, "
    "\"downloads\": true, \"fileSystems\": true, \"formData\": true, "
    "\"history\": true, \"indexedDB\": true, \"localStorage\": true, "
    "\"serverBoundCertificates\": true, \"passwords\": true, "
    "\"pluginData\": true, \"webSQL\": true"
    "}]";

// The hosted-app-data pref and UI checkbox don't correspond to a separate
// data type, but we need a way to pass that pref into the test function, so we
// use an enum value guaranteed not to conflict with existing data types.
// TODO(pamg): Update this to a newly created boundary value when the history-
// deletion policy changes have landed.
const int REMOVE_HOSTED_APP_DATA =
    BrowsingDataRemover::REMOVE_CONTENT_LICENSES << 1;

class ExtensionBrowsingDataTest : public InProcessBrowserTest,
                                  public content::NotificationObserver {
 public:
  base::Time GetBeginTime() {
    return called_with_details_->removal_begin;
  }

  int GetRemovalMask() {
    return called_with_details_->removal_mask;
  }

  int GetOriginSetMask() {
    return called_with_details_->origin_set_mask;
  }

 protected:
  virtual void SetUpOnMainThread() OVERRIDE {
    called_with_details_.reset(new BrowsingDataRemover::NotificationDetails());
    registrar_.Add(this, chrome::NOTIFICATION_BROWSING_DATA_REMOVED,
                   content::Source<Profile>(browser()->profile()));
  }

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE {
    DCHECK_EQ(type, chrome::NOTIFICATION_BROWSING_DATA_REMOVED);

    // We're not taking ownership of the details object, but storing a copy of
    // it locally.
    called_with_details_.reset(new BrowsingDataRemover::NotificationDetails(
        *content::Details<BrowsingDataRemover::NotificationDetails>(
            details).ptr()));
  }

  int GetAsMask(const DictionaryValue* dict, std::string path, int mask_value) {
    bool result;
    EXPECT_TRUE(dict->GetBoolean(path, &result));
    return result ? mask_value : 0;
  }

  void RunRemoveBrowsingDataFunctionAndCompareRemovalMask(
      const std::string& key,
      int expected_mask) {
    scoped_refptr<RemoveBrowsingDataFunction> function =
        new RemoveBrowsingDataFunction();
    SCOPED_TRACE(key);
    EXPECT_EQ(NULL, RunFunctionAndReturnSingleResult(
        function.get(),
        std::string("[{\"since\": 1}, {\"") + key + "\": true}]",
        browser()));
    EXPECT_EQ(expected_mask, GetRemovalMask());
    EXPECT_EQ(UNPROTECTED_WEB, GetOriginSetMask());
  }

  void RunRemoveBrowsingDataFunctionAndCompareOriginSetMask(
      const std::string& protectedStr,
      int expected_mask) {
    scoped_refptr<RemoveBrowsingDataFunction> function =
        new RemoveBrowsingDataFunction();
    SCOPED_TRACE(protectedStr);
    EXPECT_EQ(NULL, RunFunctionAndReturnSingleResult(
        function.get(),
        "[{\"originTypes\": " + protectedStr + "}, {\"cookies\": true}]",
        browser()));
    EXPECT_EQ(expected_mask, GetOriginSetMask());
  }

  template<class ShortcutFunction>
  void RunAndCompareRemovalMask(int expected_mask) {
    scoped_refptr<ShortcutFunction> function =
        new ShortcutFunction();
    SCOPED_TRACE(ShortcutFunction::function_name());
    EXPECT_EQ(NULL, RunFunctionAndReturnSingleResult(
        function.get(),
        std::string("[{\"since\": 1}]"),
        browser()));
    EXPECT_EQ(expected_mask, GetRemovalMask());
    EXPECT_EQ(UNPROTECTED_WEB, GetOriginSetMask());
  }

  void SetSinceAndVerify(BrowsingDataRemover::TimePeriod since_pref) {
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetInteger(prefs::kDeleteTimePeriod, since_pref);

    scoped_refptr<BrowsingDataSettingsFunction> function =
        new BrowsingDataSettingsFunction();
    SCOPED_TRACE("settings");
    scoped_ptr<base::Value> result_value(RunFunctionAndReturnSingleResult(
        function.get(), std::string("[]"), browser()));

    base::DictionaryValue* result;
    EXPECT_TRUE(result_value->GetAsDictionary(&result));
    base::DictionaryValue* options;
    EXPECT_TRUE(result->GetDictionary("options", &options));
    double since;
    EXPECT_TRUE(options->GetDouble("since", &since));

    double expected_since = 0;
    if (since_pref != BrowsingDataRemover::EVERYTHING) {
      base::Time time =
          BrowsingDataRemover::CalculateBeginDeleteTime(since_pref);
      expected_since = time.ToJsTime();
    }
    // Even a synchronous function takes nonzero time, but the difference
    // between when the function was called and now should be well under a
    // second, so we'll make sure the requested start time is within 10 seconds.
    // Since the smallest selectable period is an hour, that should be
    // sufficient.
    EXPECT_LE(expected_since, since + 10.0 * 1000.0);
  }

  void SetPrefsAndVerifySettings(int data_type_flags,
                                 int expected_origin_set_mask,
                                 int expected_removal_mask) {
    PrefService* prefs = browser()->profile()->GetPrefs();
    prefs->SetBoolean(prefs::kDeleteCache,
        !!(data_type_flags & BrowsingDataRemover::REMOVE_CACHE));
    prefs->SetBoolean(prefs::kDeleteCookies,
        !!(data_type_flags & BrowsingDataRemover::REMOVE_COOKIES));
    prefs->SetBoolean(prefs::kDeleteBrowsingHistory,
        !!(data_type_flags & BrowsingDataRemover::REMOVE_HISTORY));
    prefs->SetBoolean(prefs::kDeleteFormData,
        !!(data_type_flags & BrowsingDataRemover::REMOVE_FORM_DATA));
    prefs->SetBoolean(prefs::kDeleteDownloadHistory,
        !!(data_type_flags & BrowsingDataRemover::REMOVE_DOWNLOADS));
    prefs->SetBoolean(prefs::kDeleteHostedAppsData,
        !!(data_type_flags & REMOVE_HOSTED_APP_DATA));
    prefs->SetBoolean(prefs::kDeletePasswords,
        !!(data_type_flags & BrowsingDataRemover::REMOVE_PASSWORDS));
    prefs->SetBoolean(prefs::kClearPluginLSODataEnabled,
        !!(data_type_flags & BrowsingDataRemover::REMOVE_PLUGIN_DATA));

    scoped_refptr<BrowsingDataSettingsFunction> function =
        new BrowsingDataSettingsFunction();
    SCOPED_TRACE("settings");
    scoped_ptr<base::Value> result_value(RunFunctionAndReturnSingleResult(
        function.get(), std::string("[]"), browser()));

    base::DictionaryValue* result;
    EXPECT_TRUE(result_value->GetAsDictionary(&result));
    base::DictionaryValue* options;
    EXPECT_TRUE(result->GetDictionary("options", &options));
    base::DictionaryValue* data_to_remove;
    EXPECT_TRUE(result->GetDictionary("dataToRemove", &data_to_remove));

    base::DictionaryValue* origin_types;
    EXPECT_TRUE(options->GetDictionary("originTypes", &origin_types));

    int origin_set_mask = GetAsMask(origin_types, "unprotectedWeb",
                                    UNPROTECTED_WEB) |
                          GetAsMask(origin_types, "protectedWeb",
                                    PROTECTED_WEB) |
                          GetAsMask(origin_types, "extension", EXTENSION);
    EXPECT_EQ(expected_origin_set_mask, origin_set_mask);

    int removal_mask = GetAsMask(data_to_remove, "appcache",
                                 BrowsingDataRemover::REMOVE_APPCACHE) |
                       GetAsMask(data_to_remove, "cache",
                                 BrowsingDataRemover::REMOVE_CACHE) |
                       GetAsMask(data_to_remove, "cookies",
                                 BrowsingDataRemover::REMOVE_COOKIES) |
                       GetAsMask(data_to_remove, "downloads",
                                 BrowsingDataRemover::REMOVE_DOWNLOADS) |
                       GetAsMask(data_to_remove, "fileSystems",
                                 BrowsingDataRemover::REMOVE_FILE_SYSTEMS) |
                       GetAsMask(data_to_remove, "formData",
                                 BrowsingDataRemover::REMOVE_FORM_DATA) |
                       GetAsMask(data_to_remove, "history",
                                 BrowsingDataRemover::REMOVE_HISTORY) |
                       GetAsMask(data_to_remove, "indexedDB",
                                 BrowsingDataRemover::REMOVE_INDEXEDDB) |
                       GetAsMask(data_to_remove, "localStorage",
                                 BrowsingDataRemover::REMOVE_LOCAL_STORAGE) |
                       GetAsMask(data_to_remove, "pluginData",
                                 BrowsingDataRemover::REMOVE_PLUGIN_DATA) |
                       GetAsMask(data_to_remove, "passwords",
                                 BrowsingDataRemover::REMOVE_PASSWORDS) |
                       GetAsMask(data_to_remove, "webSQL",
                                 BrowsingDataRemover::REMOVE_WEBSQL) |
                       GetAsMask(data_to_remove, "serverBoundCertificates",
                           BrowsingDataRemover::REMOVE_SERVER_BOUND_CERTS);
    EXPECT_EQ(expected_removal_mask, removal_mask);
  }

 private:
  scoped_ptr<BrowsingDataRemover::NotificationDetails> called_with_details_;
  content::NotificationRegistrar registrar_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, OneAtATime) {
  BrowsingDataRemover::set_removing(true);
  scoped_refptr<RemoveBrowsingDataFunction> function =
      new RemoveBrowsingDataFunction();
  EXPECT_TRUE(MatchPattern(
      RunFunctionAndReturnError(function,
                                kRemoveEverythingArguments,
                                browser()),
      extension_browsing_data_api_constants::kOneAtATimeError));
  BrowsingDataRemover::set_removing(false);

  EXPECT_EQ(base::Time(), GetBeginTime());
  EXPECT_EQ(-1, GetRemovalMask());
}

// Use-after-free, see http://crbug.com/116522
IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest,
                       DISABLED_RemoveBrowsingDataAll) {
  scoped_refptr<RemoveBrowsingDataFunction> function =
      new RemoveBrowsingDataFunction();
  EXPECT_EQ(NULL, RunFunctionAndReturnSingleResult(function.get(),
                                                   kRemoveEverythingArguments,
                                                   browser()));

  EXPECT_EQ(base::Time::FromDoubleT(1.0), GetBeginTime());
  EXPECT_EQ((BrowsingDataRemover::REMOVE_SITE_DATA |
      BrowsingDataRemover::REMOVE_CACHE |
      BrowsingDataRemover::REMOVE_DOWNLOADS |
      BrowsingDataRemover::REMOVE_FORM_DATA |
      BrowsingDataRemover::REMOVE_HISTORY |
      BrowsingDataRemover::REMOVE_PASSWORDS) &
      // We can't remove plugin data inside a test profile.
      ~BrowsingDataRemover::REMOVE_PLUGIN_DATA, GetRemovalMask());
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, BrowsingDataOriginSetMask) {
  RunRemoveBrowsingDataFunctionAndCompareOriginSetMask("{}", 0);

  RunRemoveBrowsingDataFunctionAndCompareOriginSetMask(
      "{\"unprotectedWeb\": true}", UNPROTECTED_WEB);
  RunRemoveBrowsingDataFunctionAndCompareOriginSetMask(
      "{\"protectedWeb\": true}", PROTECTED_WEB);
  RunRemoveBrowsingDataFunctionAndCompareOriginSetMask(
      "{\"extension\": true}", EXTENSION);

  RunRemoveBrowsingDataFunctionAndCompareOriginSetMask(
      "{\"unprotectedWeb\": true, \"protectedWeb\": true}",
      UNPROTECTED_WEB | PROTECTED_WEB);
  RunRemoveBrowsingDataFunctionAndCompareOriginSetMask(
      "{\"unprotectedWeb\": true, \"extension\": true}",
      UNPROTECTED_WEB | EXTENSION);
  RunRemoveBrowsingDataFunctionAndCompareOriginSetMask(
      "{\"protectedWeb\": true, \"extension\": true}",
      PROTECTED_WEB | EXTENSION);

  RunRemoveBrowsingDataFunctionAndCompareOriginSetMask(
      ("{\"unprotectedWeb\": true, \"protectedWeb\": true, "
       "\"extension\": true}"),
      UNPROTECTED_WEB | PROTECTED_WEB | EXTENSION);
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, BrowsingDataRemovalMask) {
  RunRemoveBrowsingDataFunctionAndCompareRemovalMask(
      "appcache", BrowsingDataRemover::REMOVE_APPCACHE);
  RunRemoveBrowsingDataFunctionAndCompareRemovalMask(
      "cache", BrowsingDataRemover::REMOVE_CACHE);
  RunRemoveBrowsingDataFunctionAndCompareRemovalMask(
      "cookies", BrowsingDataRemover::REMOVE_COOKIES);
  RunRemoveBrowsingDataFunctionAndCompareRemovalMask(
      "downloads", BrowsingDataRemover::REMOVE_DOWNLOADS);
  RunRemoveBrowsingDataFunctionAndCompareRemovalMask(
      "fileSystems", BrowsingDataRemover::REMOVE_FILE_SYSTEMS);
  RunRemoveBrowsingDataFunctionAndCompareRemovalMask(
      "formData", BrowsingDataRemover::REMOVE_FORM_DATA);
  RunRemoveBrowsingDataFunctionAndCompareRemovalMask(
      "history", BrowsingDataRemover::REMOVE_HISTORY);
  RunRemoveBrowsingDataFunctionAndCompareRemovalMask(
      "indexedDB", BrowsingDataRemover::REMOVE_INDEXEDDB);
  RunRemoveBrowsingDataFunctionAndCompareRemovalMask(
      "localStorage", BrowsingDataRemover::REMOVE_LOCAL_STORAGE);
  RunRemoveBrowsingDataFunctionAndCompareRemovalMask(
      "serverBoundCertificates",
      BrowsingDataRemover::REMOVE_SERVER_BOUND_CERTS);
  RunRemoveBrowsingDataFunctionAndCompareRemovalMask(
      "passwords", BrowsingDataRemover::REMOVE_PASSWORDS);
  // We can't remove plugin data inside a test profile.
  RunRemoveBrowsingDataFunctionAndCompareRemovalMask(
      "webSQL", BrowsingDataRemover::REMOVE_WEBSQL);
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, ShortcutFunctionRemovalMask) {
  RunAndCompareRemovalMask<RemoveAppCacheFunction>(
      BrowsingDataRemover::REMOVE_APPCACHE);
  RunAndCompareRemovalMask<RemoveCacheFunction>(
      BrowsingDataRemover::REMOVE_CACHE);
  RunAndCompareRemovalMask<RemoveCookiesFunction>(
      BrowsingDataRemover::REMOVE_COOKIES |
      BrowsingDataRemover::REMOVE_SERVER_BOUND_CERTS);
  RunAndCompareRemovalMask<RemoveDownloadsFunction>(
      BrowsingDataRemover::REMOVE_DOWNLOADS);
  RunAndCompareRemovalMask<RemoveFileSystemsFunction>(
      BrowsingDataRemover::REMOVE_FILE_SYSTEMS);
  RunAndCompareRemovalMask<RemoveFormDataFunction>(
      BrowsingDataRemover::REMOVE_FORM_DATA);
  RunAndCompareRemovalMask<RemoveHistoryFunction>(
      BrowsingDataRemover::REMOVE_HISTORY);
  RunAndCompareRemovalMask<RemoveIndexedDBFunction>(
      BrowsingDataRemover::REMOVE_INDEXEDDB);
  RunAndCompareRemovalMask<RemoveLocalStorageFunction>(
      BrowsingDataRemover::REMOVE_LOCAL_STORAGE);
  // We can't remove plugin data inside a test profile.
  RunAndCompareRemovalMask<RemovePasswordsFunction>(
      BrowsingDataRemover::REMOVE_PASSWORDS);
  RunAndCompareRemovalMask<RemoveWebSQLFunction>(
      BrowsingDataRemover::REMOVE_WEBSQL);
}

// Test the processing of the 'delete since' preference.
IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, SettingsFunctionSince) {
  SetSinceAndVerify(BrowsingDataRemover::EVERYTHING);
  SetSinceAndVerify(BrowsingDataRemover::LAST_HOUR);
  SetSinceAndVerify(BrowsingDataRemover::LAST_DAY);
  SetSinceAndVerify(BrowsingDataRemover::LAST_WEEK);
  SetSinceAndVerify(BrowsingDataRemover::FOUR_WEEKS);
}

IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, SettingsFunctionEmpty) {
  SetPrefsAndVerifySettings(0, 0, 0);
}

// Test straightforward settings, mapped 1:1 to data types.
IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, SettingsFunctionSimple) {
  SetPrefsAndVerifySettings(BrowsingDataRemover::REMOVE_CACHE, 0,
                            BrowsingDataRemover::REMOVE_CACHE);
  SetPrefsAndVerifySettings(BrowsingDataRemover::REMOVE_HISTORY, 0,
                            BrowsingDataRemover::REMOVE_HISTORY);
  SetPrefsAndVerifySettings(BrowsingDataRemover::REMOVE_FORM_DATA, 0,
                            BrowsingDataRemover::REMOVE_FORM_DATA);
  SetPrefsAndVerifySettings(BrowsingDataRemover::REMOVE_DOWNLOADS, 0,
                            BrowsingDataRemover::REMOVE_DOWNLOADS);
  SetPrefsAndVerifySettings(BrowsingDataRemover::REMOVE_PASSWORDS, 0,
                            BrowsingDataRemover::REMOVE_PASSWORDS);
}

// Test cookie and app data settings.
IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, SettingsFunctionSiteData) {
  int site_data_no_plugins = BrowsingDataRemover::REMOVE_SITE_DATA &
      ~BrowsingDataRemover::REMOVE_PLUGIN_DATA;

  SetPrefsAndVerifySettings(BrowsingDataRemover::REMOVE_COOKIES,
                            UNPROTECTED_WEB,
                            site_data_no_plugins);
  SetPrefsAndVerifySettings(REMOVE_HOSTED_APP_DATA,
                           PROTECTED_WEB,
                           site_data_no_plugins);
  SetPrefsAndVerifySettings(
      BrowsingDataRemover::REMOVE_COOKIES | REMOVE_HOSTED_APP_DATA,
      PROTECTED_WEB | UNPROTECTED_WEB,
      site_data_no_plugins);
  SetPrefsAndVerifySettings(
      BrowsingDataRemover::REMOVE_COOKIES |
          BrowsingDataRemover::REMOVE_PLUGIN_DATA,
      UNPROTECTED_WEB,
      BrowsingDataRemover::REMOVE_SITE_DATA);
}

// Test an arbitrary assortment of settings.
IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, SettingsFunctionAssorted) {
  int site_data_no_plugins = BrowsingDataRemover::REMOVE_SITE_DATA &
      ~BrowsingDataRemover::REMOVE_PLUGIN_DATA;

  SetPrefsAndVerifySettings(
      BrowsingDataRemover::REMOVE_COOKIES |
          BrowsingDataRemover::REMOVE_HISTORY |
          BrowsingDataRemover::REMOVE_DOWNLOADS,
    UNPROTECTED_WEB,
    site_data_no_plugins |
        BrowsingDataRemover::REMOVE_HISTORY |
        BrowsingDataRemover::REMOVE_DOWNLOADS);
}
