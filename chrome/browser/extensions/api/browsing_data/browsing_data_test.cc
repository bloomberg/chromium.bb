// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/browsing_data/browsing_data_api.h"

#include <string>

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/string_util.h"
#include "base/stringprintf.h"
#include "base/values.h"
#include "chrome/browser/browsing_data_helper.h"
#include "chrome/browser/browsing_data_remover.h"
#include "chrome/browser/extensions/extension_function_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "content/public/browser/notification_service.h"

using extension_function_test_utils::RunFunctionAndReturnError;
using extension_function_test_utils::RunFunctionAndReturnResult;

namespace {

const char kRemoveEverythingArguments[] = "[{\"since\": 1000}, {"
    "\"appcache\": true, \"cache\": true, \"cookies\": true, "
    "\"downloads\": true, \"fileSystems\": true, \"formData\": true, "
    "\"history\": true, \"indexedDB\": true, \"localStorage\": true, "
    "\"serverBoundCerts\": true, \"passwords\": true, \"pluginData\": true, "
    "\"webSQL\": true"
    "}]";

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
  virtual void SetUpOnMainThread() {
    called_with_details_.reset(new BrowsingDataRemover::NotificationDetails());
    registrar_.Add(this, chrome::NOTIFICATION_BROWSING_DATA_REMOVED,
                   content::Source<Profile>(browser()->profile()));
  }

  virtual void TearDownOnMainThread() {
    registrar_.RemoveAll();
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

  void RunRemoveBrowsingDataFunctionAndCompareRemovalMask(
      const std::string& key,
      int expected_mask) {
    SCOPED_TRACE(key);
    EXPECT_EQ(NULL, RunFunctionAndReturnResult(new RemoveBrowsingDataFunction(),
        std::string("[{\"since\": 1}, {\"") + key + "\": true}]", browser()));
    EXPECT_EQ(expected_mask, GetRemovalMask());
    EXPECT_EQ(BrowsingDataHelper::UNPROTECTED_WEB, GetOriginSetMask());
  }

  void RunRemoveBrowsingDataFunctionAndCompareOriginSetMask(
      const std::string& protectedStr,
      int expected_mask) {
    SCOPED_TRACE(protectedStr);
    EXPECT_EQ(NULL, RunFunctionAndReturnResult(new RemoveBrowsingDataFunction(),
        "[{\"originType\": " + protectedStr + "}, {\"cookies\": true}]",
        browser()));
    EXPECT_EQ(expected_mask, GetOriginSetMask());
  }

 private:
  scoped_ptr<BrowsingDataRemover::NotificationDetails> called_with_details_;
  content::NotificationRegistrar registrar_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, OneAtATime) {
  BrowsingDataRemover::set_removing(true);
  EXPECT_TRUE(MatchPattern(
      RunFunctionAndReturnError(
          new RemoveBrowsingDataFunction(),
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
  EXPECT_EQ(NULL, RunFunctionAndReturnResult(new RemoveBrowsingDataFunction(),
      kRemoveEverythingArguments, browser()));

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
      "{\"unprotectedWeb\": true}", BrowsingDataHelper::UNPROTECTED_WEB);
  RunRemoveBrowsingDataFunctionAndCompareOriginSetMask(
      "{\"protectedWeb\": true}", BrowsingDataHelper::PROTECTED_WEB);
  RunRemoveBrowsingDataFunctionAndCompareOriginSetMask(
      "{\"extension\": true}", BrowsingDataHelper::EXTENSION);

  RunRemoveBrowsingDataFunctionAndCompareOriginSetMask(
      "{\"unprotectedWeb\": true, \"protectedWeb\": true}",
      BrowsingDataHelper::UNPROTECTED_WEB | BrowsingDataHelper::PROTECTED_WEB);
  RunRemoveBrowsingDataFunctionAndCompareOriginSetMask(
      "{\"unprotectedWeb\": true, \"extension\": true}",
      BrowsingDataHelper::UNPROTECTED_WEB | BrowsingDataHelper::EXTENSION);
  RunRemoveBrowsingDataFunctionAndCompareOriginSetMask(
      "{\"protectedWeb\": true, \"extension\": true}",
      BrowsingDataHelper::PROTECTED_WEB | BrowsingDataHelper::EXTENSION);

  RunRemoveBrowsingDataFunctionAndCompareOriginSetMask(
      ("{\"unprotectedWeb\": true, \"protectedWeb\": true, "
       "\"extension\": true}"),
      BrowsingDataHelper::UNPROTECTED_WEB | BrowsingDataHelper::PROTECTED_WEB |
      BrowsingDataHelper::EXTENSION);
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
      "serverBoundCerts", BrowsingDataRemover::REMOVE_SERVER_BOUND_CERTS);
  RunRemoveBrowsingDataFunctionAndCompareRemovalMask(
      "passwords", BrowsingDataRemover::REMOVE_PASSWORDS);
  // We can't remove plugin data inside a test profile.
  RunRemoveBrowsingDataFunctionAndCompareRemovalMask(
      "webSQL", BrowsingDataRemover::REMOVE_WEBSQL);
}
