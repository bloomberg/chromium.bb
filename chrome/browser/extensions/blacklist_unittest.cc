// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop.h"
#include "base/run_loop.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/test_blacklist.h"
#include "chrome/browser/extensions/test_extension_prefs.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace extensions {
namespace {

class BlacklistTest : public testing::Test {
 public:
  BlacklistTest()
      : prefs_(message_loop_.message_loop_proxy()),
        blacklist_(prefs_.prefs()) {
  }

  bool IsBlacklisted(const Extension* extension) {
    return TestBlacklist(&blacklist_).IsBlacklisted(extension->id());
  }

 protected:
  MessageLoop message_loop_;

  TestExtensionPrefs prefs_;

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

}  // namespace extensions
}  // namespace
