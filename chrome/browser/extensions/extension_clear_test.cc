// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/extension_clear_api.h"

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

using namespace extension_function_test_utils;

namespace {

const char kClearEverythingArguments[] = "[1000, {"
    "\"appcache\": true, \"cache\": true, \"cookies\": true, "
    "\"downloads\": true, \"fileSystems\": true, \"formData\": true, "
    "\"history\": true, \"indexedDB\": true, \"localStorage\": true, "
    "\"pluginData\": true, \"passwords\": true, \"webSQL\": true"
    "}]";

class ExtensionClearTest : public InProcessBrowserTest,
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

    registrar_.RemoveAll();
  }

 private:
  scoped_ptr<BrowsingDataRemover::NotificationDetails> called_with_details_;
  content::NotificationRegistrar registrar_;
};

}  // namespace

IN_PROC_BROWSER_TEST_F(ExtensionClearTest, OneAtATime) {
  BrowsingDataRemover::set_removing(true);
  EXPECT_TRUE(MatchPattern(
      RunFunctionAndReturnError(
          new ClearBrowsingDataFunction(),
          kClearEverythingArguments,
          browser()),
      extension_clear_api_constants::kOneAtATimeError));
  BrowsingDataRemover::set_removing(false);

  EXPECT_EQ(base::Time(), GetBeginTime());
  EXPECT_EQ(-1, GetRemovalMask());
}

IN_PROC_BROWSER_TEST_F(ExtensionClearTest, ClearBrowsingDataEverything) {
  EXPECT_EQ(NULL, RunFunctionAndReturnResult(new ClearBrowsingDataFunction(),
      kClearEverythingArguments, browser()));

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
