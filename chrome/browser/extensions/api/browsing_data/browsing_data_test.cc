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

  void RunRemoveBrowsingDataFunctionAndCompareMask(const std::string& key,
                                                  int expected_mask) {
    SCOPED_TRACE(key);
    EXPECT_EQ(NULL, RunFunctionAndReturnResult(new RemoveBrowsingDataFunction(),
        std::string("[{\"since\": 1}, {\"") + key + "\": true}]", browser()));
    EXPECT_EQ(expected_mask, GetRemovalMask());
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

IN_PROC_BROWSER_TEST_F(ExtensionBrowsingDataTest, RemoveBrowsingDataMask) {
  RunRemoveBrowsingDataFunctionAndCompareMask(
      "appcache", BrowsingDataRemover::REMOVE_APPCACHE);
  RunRemoveBrowsingDataFunctionAndCompareMask(
      "cache", BrowsingDataRemover::REMOVE_CACHE);
  RunRemoveBrowsingDataFunctionAndCompareMask(
      "cookies", BrowsingDataRemover::REMOVE_COOKIES);
  RunRemoveBrowsingDataFunctionAndCompareMask(
      "downloads", BrowsingDataRemover::REMOVE_DOWNLOADS);
  RunRemoveBrowsingDataFunctionAndCompareMask(
      "fileSystems", BrowsingDataRemover::REMOVE_FILE_SYSTEMS);
  RunRemoveBrowsingDataFunctionAndCompareMask(
      "formData", BrowsingDataRemover::REMOVE_FORM_DATA);
  RunRemoveBrowsingDataFunctionAndCompareMask(
      "history", BrowsingDataRemover::REMOVE_HISTORY);
  RunRemoveBrowsingDataFunctionAndCompareMask(
      "indexedDB", BrowsingDataRemover::REMOVE_INDEXEDDB);
  RunRemoveBrowsingDataFunctionAndCompareMask(
      "localStorage", BrowsingDataRemover::REMOVE_LOCAL_STORAGE);
  RunRemoveBrowsingDataFunctionAndCompareMask(
      "serverBoundCerts", BrowsingDataRemover::REMOVE_SERVER_BOUND_CERTS);
  RunRemoveBrowsingDataFunctionAndCompareMask(
      "passwords", BrowsingDataRemover::REMOVE_PASSWORDS);
  // We can't remove plugin data inside a test profile.
  RunRemoveBrowsingDataFunctionAndCompareMask(
      "webSQL", BrowsingDataRemover::REMOVE_WEBSQL);
}
