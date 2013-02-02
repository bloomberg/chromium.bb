// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/fake_safe_browsing_database_manager.h"
#include "chrome/browser/extensions/test_blacklist.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "content/public/test/test_browser_thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

class BlacklistTest : public testing::Test {
 public:
  BlacklistTest()
      : prefs_(message_loop_.message_loop_proxy()),
        ui_thread_(content::BrowserThread::UI, &message_loop_),
        io_thread_(content::BrowserThread::IO, &message_loop_),
        safe_browsing_database_manager_(
            new FakeSafeBrowsingDatabaseManager()),
        scoped_blacklist_database_manager_(safe_browsing_database_manager_),
        blacklist_(prefs_.prefs()) {
  }

  bool IsBlacklisted(const Extension* extension) {
    return TestBlacklist(&blacklist_).IsBlacklisted(extension->id());
  }

 protected:
  MessageLoop message_loop_;

  TestExtensionPrefs prefs_;

  content::TestBrowserThread ui_thread_;
  content::TestBrowserThread io_thread_;

  scoped_refptr<FakeSafeBrowsingDatabaseManager>
      safe_browsing_database_manager_;
  Blacklist::ScopedDatabaseManagerForTest scoped_blacklist_database_manager_;

  Blacklist blacklist_;
};

TEST_F(BlacklistTest, SetFromUpdater) {
  scoped_refptr<const Extension> extension_a = prefs_.AddExtension("a");
  scoped_refptr<const Extension> extension_b = prefs_.AddExtension("b");
  scoped_refptr<const Extension> extension_c = prefs_.AddExtension("c");
  scoped_refptr<const Extension> extension_d = prefs_.AddExtension("d");

  // c, d, start blacklisted.
  prefs_.prefs()->SetExtensionBlacklisted(extension_c->id(), true);
  prefs_.prefs()->SetExtensionBlacklisted(extension_d->id(), true);

  EXPECT_FALSE(IsBlacklisted(extension_a));
  EXPECT_FALSE(IsBlacklisted(extension_b));
  EXPECT_TRUE(IsBlacklisted(extension_c));
  EXPECT_TRUE(IsBlacklisted(extension_d));

  // Mix up the blacklist.
  {
    std::vector<std::string> blacklist;
    blacklist.push_back(extension_b->id());
    blacklist.push_back(extension_c->id());
    blacklist_.SetFromUpdater(blacklist, "1");
  }
  EXPECT_FALSE(IsBlacklisted(extension_a));
  EXPECT_TRUE(IsBlacklisted(extension_b));
  EXPECT_TRUE(IsBlacklisted(extension_c));
  EXPECT_FALSE(IsBlacklisted(extension_d));

  // No-op, just in case.
  {
    std::vector<std::string> blacklist;
    blacklist.push_back(extension_b->id());
    blacklist.push_back(extension_c->id());
    blacklist_.SetFromUpdater(blacklist, "2");
  }
  EXPECT_FALSE(IsBlacklisted(extension_a));
  EXPECT_TRUE(IsBlacklisted(extension_b));
  EXPECT_TRUE(IsBlacklisted(extension_c));
  EXPECT_FALSE(IsBlacklisted(extension_d));

  // Strictly increase the blacklist.
  {
    std::vector<std::string> blacklist;
    blacklist.push_back(extension_a->id());
    blacklist.push_back(extension_b->id());
    blacklist.push_back(extension_c->id());
    blacklist.push_back(extension_d->id());
    blacklist_.SetFromUpdater(blacklist, "3");
  }
  EXPECT_TRUE(IsBlacklisted(extension_a));
  EXPECT_TRUE(IsBlacklisted(extension_b));
  EXPECT_TRUE(IsBlacklisted(extension_c));
  EXPECT_TRUE(IsBlacklisted(extension_d));

  // Strictly decrease the blacklist.
  {
    std::vector<std::string> blacklist;
    blacklist.push_back(extension_a->id());
    blacklist.push_back(extension_b->id());
    blacklist_.SetFromUpdater(blacklist, "4");
  }
  EXPECT_TRUE(IsBlacklisted(extension_a));
  EXPECT_TRUE(IsBlacklisted(extension_b));
  EXPECT_FALSE(IsBlacklisted(extension_c));
  EXPECT_FALSE(IsBlacklisted(extension_d));

  // Clear the blacklist.
  {
    std::vector<std::string> blacklist;
    blacklist_.SetFromUpdater(blacklist, "5");
  }
  EXPECT_FALSE(IsBlacklisted(extension_a));
  EXPECT_FALSE(IsBlacklisted(extension_b));
  EXPECT_FALSE(IsBlacklisted(extension_c));
  EXPECT_FALSE(IsBlacklisted(extension_d));
}

void Assign(std::set<std::string> *to, const std::set<std::string>& from) {
  *to = from;
}

TEST_F(BlacklistTest, OnlyIncludesRequestedIDs) {
  scoped_refptr<const Extension> extension_a = prefs_.AddExtension("a");
  scoped_refptr<const Extension> extension_b = prefs_.AddExtension("b");
  scoped_refptr<const Extension> extension_c = prefs_.AddExtension("c");

  {
    std::vector<std::string> blacklist;
    blacklist.push_back(extension_a->id());
    blacklist.push_back(extension_b->id());
    blacklist_.SetFromUpdater(blacklist, "1");
    base::RunLoop().RunUntilIdle();
  }

  std::set<std::string> blacklist_actual;
  {
    std::set<std::string> blacklist_query;
    blacklist_query.insert(extension_a->id());
    blacklist_query.insert(extension_c->id());
    blacklist_.GetBlacklistedIDs(blacklist_query,
                                 base::Bind(&Assign, &blacklist_actual));
    base::RunLoop().RunUntilIdle();
  }

  std::set<std::string> blacklist_expected;
  blacklist_expected.insert(extension_a->id());
  EXPECT_EQ(blacklist_expected, blacklist_actual);
}

TEST_F(BlacklistTest, PrefsVsSafeBrowsing) {
  scoped_refptr<const Extension> extension_a = prefs_.AddExtension("a");
  scoped_refptr<const Extension> extension_b = prefs_.AddExtension("b");
  scoped_refptr<const Extension> extension_c = prefs_.AddExtension("c");

  // Prefs have a and b blacklisted, safebrowsing has b and c.
  prefs_.prefs()->SetExtensionBlacklisted(extension_a->id(), true);
  prefs_.prefs()->SetExtensionBlacklisted(extension_b->id(), true);
  {
    std::set<std::string> bc;
    bc.insert(extension_b->id());
    bc.insert(extension_c->id());
    safe_browsing_database_manager_->set_unsafe_ids(bc);
  }

  // The manager is still disabled at this point, so c won't be blacklisted.
  EXPECT_TRUE(IsBlacklisted(extension_a));
  EXPECT_TRUE(IsBlacklisted(extension_b));
  EXPECT_FALSE(IsBlacklisted(extension_c));

  // Now it should be.
  safe_browsing_database_manager_->set_enabled(true);
  EXPECT_TRUE(IsBlacklisted(extension_a));
  EXPECT_TRUE(IsBlacklisted(extension_b));
  EXPECT_TRUE(IsBlacklisted(extension_c));

  // Corner case: nothing in safebrowsing (but still enabled).
  safe_browsing_database_manager_->set_unsafe_ids(std::set<std::string>());
  EXPECT_TRUE(IsBlacklisted(extension_a));
  EXPECT_TRUE(IsBlacklisted(extension_b));
  EXPECT_FALSE(IsBlacklisted(extension_c));

  // Corner case: nothing in prefs.
  prefs_.prefs()->SetExtensionBlacklisted(extension_a->id(), false);
  prefs_.prefs()->SetExtensionBlacklisted(extension_b->id(), false);
  {
    std::set<std::string> bc;
    bc.insert(extension_b->id());
    bc.insert(extension_c->id());
    safe_browsing_database_manager_->set_unsafe_ids(bc);
  }
  EXPECT_FALSE(IsBlacklisted(extension_a));
  EXPECT_TRUE(IsBlacklisted(extension_b));
  EXPECT_TRUE(IsBlacklisted(extension_c));
}

}  // namespace extensions
}  // namespace
