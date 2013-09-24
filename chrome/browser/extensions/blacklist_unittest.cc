// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/fake_safe_browsing_database_manager.h"
#include "chrome/browser/extensions/test_blacklist.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "content/public/test/test_browser_thread_bundle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

std::set<std::string> Set(const std::string& a) {
  std::set<std::string> set;
  set.insert(a);
  return set;
}
std::set<std::string> Set(const std::string& a, const std::string& b) {
  std::set<std::string> set = Set(a);
  set.insert(b);
  return set;
}
std::set<std::string> Set(const std::string& a,
                          const std::string& b,
                          const std::string& c) {
  std::set<std::string> set = Set(a, b);
  set.insert(c);
  return set;
}
std::set<std::string> Set(const std::string& a,
                          const std::string& b,
                          const std::string& d,
                          const std::string& c) {
  std::set<std::string> set = Set(a, b, c);
  set.insert(d);
  return set;
}

class BlacklistTest : public testing::Test {
 public:
  BlacklistTest()
      : test_prefs_(base::MessageLoopProxy::current()),
        blacklist_db_(new FakeSafeBrowsingDatabaseManager(false)),
        scoped_blacklist_db_(blacklist_db_) {}

 protected:
  ExtensionPrefs* prefs() {
    return test_prefs_.prefs();
  }

  FakeSafeBrowsingDatabaseManager* blacklist_db() {
    return blacklist_db_.get();
  }

  std::string AddExtension(const std::string& id) {
    return test_prefs_.AddExtension(id)->id();
  }

 private:
  content::TestBrowserThreadBundle browser_thread_bundle_;

  TestExtensionPrefs test_prefs_;

  scoped_refptr<FakeSafeBrowsingDatabaseManager> blacklist_db_;

  Blacklist::ScopedDatabaseManagerForTest scoped_blacklist_db_;
};

void Assign(std::set<std::string> *to, const std::set<std::string>& from) {
  *to = from;
}

TEST_F(BlacklistTest, OnlyIncludesRequestedIDs) {
  std::string a = AddExtension("a");
  std::string b = AddExtension("b");
  std::string c = AddExtension("c");

  Blacklist blacklist(prefs());
  TestBlacklist tester(&blacklist);

  blacklist_db()->Enable();
  blacklist_db()->SetUnsafe(a, b);

  EXPECT_TRUE(tester.IsBlacklisted(a));
  EXPECT_TRUE(tester.IsBlacklisted(b));
  EXPECT_FALSE(tester.IsBlacklisted(c));

  std::set<std::string> blacklisted_ids;
  blacklist.GetBlacklistedIDs(Set(a, c), base::Bind(&Assign, &blacklisted_ids));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(Set(a), blacklisted_ids);
}

TEST_F(BlacklistTest, SafeBrowsing) {
  std::string a = AddExtension("a");

  Blacklist blacklist(prefs());
  TestBlacklist tester(&blacklist);

  EXPECT_FALSE(tester.IsBlacklisted(a));

  blacklist_db()->SetUnsafe(a);
  // The manager is still disabled at this point, so it won't be blacklisted.
  EXPECT_FALSE(tester.IsBlacklisted(a));

  blacklist_db()->Enable().NotifyUpdate();
  base::RunLoop().RunUntilIdle();
  // Now it should be.
  EXPECT_TRUE(tester.IsBlacklisted(a));

  blacklist_db()->ClearUnsafe().NotifyUpdate();
  // Safe browsing blacklist empty, now enabled.
  EXPECT_FALSE(tester.IsBlacklisted(a));
}

// Tests that Blacklist clears the old prefs blacklist on startup.
TEST_F(BlacklistTest, ClearsPreferencesBlacklist) {
  std::string a = AddExtension("a");
  std::string b = AddExtension("b");

  // Blacklist an installed extension.
  prefs()->SetExtensionBlacklisted(a, true);

  // Blacklist some non-installed extensions. This is what the old preferences
  // blacklist looked like.
  std::string c = "cccccccccccccccccccccccccccccccc";
  std::string d = "dddddddddddddddddddddddddddddddd";
  prefs()->SetExtensionBlacklisted(c, true);
  prefs()->SetExtensionBlacklisted(d, true);

  EXPECT_EQ(Set(a, c, d), prefs()->GetBlacklistedExtensions());

  Blacklist blacklist(prefs());
  TestBlacklist tester(&blacklist);

  // Blacklist has been cleared. Only the installed extension "a" left.
  EXPECT_EQ(Set(a), prefs()->GetBlacklistedExtensions());
  EXPECT_TRUE(prefs()->GetInstalledExtensionInfo(a).get());
  EXPECT_TRUE(prefs()->GetInstalledExtensionInfo(b).get());

  // "a" won't actually be *blacklisted* since it doesn't appear in
  // safebrowsing. Blacklist no longer reads from prefs. This is purely a
  // concern of somebody else (currently, ExtensionService).
  std::set<std::string> blacklisted_ids;
  blacklist.GetBlacklistedIDs(Set(a, b, c, d),
                              base::Bind(&Assign, &blacklisted_ids));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::set<std::string>(), blacklisted_ids);

  // Prefs are still unaffected for installed extensions, though.
  EXPECT_TRUE(prefs()->IsExtensionBlacklisted(a));
  EXPECT_FALSE(prefs()->IsExtensionBlacklisted(b));
  EXPECT_FALSE(prefs()->IsExtensionBlacklisted(c));
  EXPECT_FALSE(prefs()->IsExtensionBlacklisted(d));
}

}  // namespace
}  // namespace extensions
