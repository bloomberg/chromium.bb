// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/message_loop/message_loop.h"
#include "base/run_loop.h"
#include "base/stl_util.h"
#include "chrome/browser/extensions/blacklist.h"
#include "chrome/browser/extensions/blacklist_state_fetcher.h"
#include "chrome/browser/extensions/extension_prefs.h"
#include "chrome/browser/extensions/fake_safe_browsing_database_manager.h"
#include "chrome/browser/extensions/test_blacklist.h"
#include "chrome/browser/extensions/test_blacklist_state_fetcher.h"
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
                          const std::string& c,
                          const std::string& d) {
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

class BlacklistStateFetcherMock : public BlacklistStateFetcher {
 public:
  virtual void Request(const std::string& id,
                       const RequestCallback& callback) OVERRIDE {
    request_count_++;

    BlacklistState result = NOT_BLACKLISTED;
    if (ContainsKey(states_, id))
      result = states_[id];

    base::MessageLoopProxy::current()->PostTask(FROM_HERE,
                                                base::Bind(callback, result));
  }

  void SetState(const std::string& id, BlacklistState state) {
    states_[id] = state;
  }

  int request_count() const {
    return request_count_;
  }

 private:
  std::map<std::string, BlacklistState> states_;
  int request_count_;
};

template<typename T>
void Assign(T *to, const T& from) {
  *to = from;
}

}  // namespace

TEST_F(BlacklistTest, OnlyIncludesRequestedIDs) {
  std::string a = AddExtension("a");
  std::string b = AddExtension("b");
  std::string c = AddExtension("c");

  Blacklist blacklist(prefs());
  TestBlacklist tester(&blacklist);
  BlacklistStateFetcherMock* fetcher = new BlacklistStateFetcherMock();
  fetcher->SetState(a, BLACKLISTED_MALWARE);
  fetcher->SetState(b, BLACKLISTED_MALWARE);
  blacklist.SetBlacklistStateFetcherForTest(fetcher);

  blacklist_db()->Enable();
  blacklist_db()->SetUnsafe(a, b);

  EXPECT_EQ(BLACKLISTED_MALWARE, tester.GetBlacklistState(a));
  EXPECT_EQ(BLACKLISTED_MALWARE, tester.GetBlacklistState(b));
  EXPECT_EQ(NOT_BLACKLISTED, tester.GetBlacklistState(c));

  std::set<std::string> blacklisted_ids;
  blacklist.GetMalwareIDs(
      Set(a, c), base::Bind(&Assign<std::set<std::string> >, &blacklisted_ids));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(Set(a), blacklisted_ids);
}

TEST_F(BlacklistTest, SafeBrowsing) {
  std::string a = AddExtension("a");

  Blacklist blacklist(prefs());
  TestBlacklist tester(&blacklist);
  BlacklistStateFetcherMock* fetcher = new BlacklistStateFetcherMock();
  fetcher->SetState(a, BLACKLISTED_MALWARE);
  blacklist.SetBlacklistStateFetcherForTest(fetcher);

  EXPECT_EQ(NOT_BLACKLISTED, tester.GetBlacklistState(a));

  blacklist_db()->SetUnsafe(a);
  // The manager is still disabled at this point, so it won't be blacklisted.
  EXPECT_EQ(NOT_BLACKLISTED, tester.GetBlacklistState(a));

  blacklist_db()->Enable().NotifyUpdate();
  base::RunLoop().RunUntilIdle();
  // Now it should be.
  EXPECT_EQ(BLACKLISTED_MALWARE, tester.GetBlacklistState(a));

  blacklist_db()->ClearUnsafe().NotifyUpdate();
  // Safe browsing blacklist empty, now enabled.
  EXPECT_EQ(NOT_BLACKLISTED, tester.GetBlacklistState(a));
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
  blacklist.SetBlacklistStateFetcherForTest(new BlacklistStateFetcherMock());

  // Blacklist has been cleared. Only the installed extension "a" left.
  EXPECT_EQ(Set(a), prefs()->GetBlacklistedExtensions());
  EXPECT_TRUE(prefs()->GetInstalledExtensionInfo(a).get());
  EXPECT_TRUE(prefs()->GetInstalledExtensionInfo(b).get());

  // "a" won't actually be *blacklisted* since it doesn't appear in
  // safebrowsing. Blacklist no longer reads from prefs. This is purely a
  // concern of somebody else (currently, ExtensionService).
  std::set<std::string> blacklisted_ids;
  blacklist.GetMalwareIDs(Set(a, b, c, d),
                          base::Bind(&Assign<std::set<std::string> >,
                                     &blacklisted_ids));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(std::set<std::string>(), blacklisted_ids);

  // Prefs are still unaffected for installed extensions, though.
  EXPECT_TRUE(prefs()->IsExtensionBlacklisted(a));
  EXPECT_FALSE(prefs()->IsExtensionBlacklisted(b));
  EXPECT_FALSE(prefs()->IsExtensionBlacklisted(c));
  EXPECT_FALSE(prefs()->IsExtensionBlacklisted(d));
}

// Test getting different blacklist states from Blacklist.
TEST_F(BlacklistTest, GetBlacklistStates) {
  Blacklist blacklist(prefs());

  std::string a = AddExtension("a");
  std::string b = AddExtension("b");
  std::string c = AddExtension("c");
  std::string d = AddExtension("d");
  std::string e = AddExtension("e");

  blacklist_db()->Enable();
  blacklist_db()->SetUnsafe(a, b, c, d);

  // Will be deleted by blacklist destructor.
  BlacklistStateFetcherMock* fetcher = new BlacklistStateFetcherMock();
  fetcher->SetState(a, BLACKLISTED_MALWARE);
  fetcher->SetState(b, BLACKLISTED_SECURITY_VULNERABILITY);
  fetcher->SetState(c, BLACKLISTED_CWS_POLICY_VIOLATION);
  fetcher->SetState(d, BLACKLISTED_POTENTIALLY_UNWANTED);
  blacklist.SetBlacklistStateFetcherForTest(fetcher);

  Blacklist::BlacklistStateMap states_abc;
  Blacklist::BlacklistStateMap states_bcd;
  blacklist.GetBlacklistedIDs(Set(a, b, c, e),
                              base::Bind(&Assign<Blacklist::BlacklistStateMap>,
                                         &states_abc));
  blacklist.GetBlacklistedIDs(Set(b, c, d, e),
                              base::Bind(&Assign<Blacklist::BlacklistStateMap>,
                                         &states_bcd));
  base::RunLoop().RunUntilIdle();

  EXPECT_EQ(BLACKLISTED_MALWARE, states_abc[a]);
  EXPECT_EQ(BLACKLISTED_SECURITY_VULNERABILITY, states_abc[b]);
  EXPECT_EQ(BLACKLISTED_CWS_POLICY_VIOLATION, states_abc[c]);
  EXPECT_EQ(BLACKLISTED_SECURITY_VULNERABILITY, states_bcd[b]);
  EXPECT_EQ(BLACKLISTED_CWS_POLICY_VIOLATION, states_bcd[c]);
  EXPECT_EQ(BLACKLISTED_POTENTIALLY_UNWANTED, states_bcd[d]);
  EXPECT_EQ(states_abc.end(), states_abc.find(e));
  EXPECT_EQ(states_bcd.end(), states_bcd.find(e));

  int old_request_count = fetcher->request_count();
  Blacklist::BlacklistStateMap states_ad;
  blacklist.GetBlacklistedIDs(Set(a, d, e),
                              base::Bind(&Assign<Blacklist::BlacklistStateMap>,
                                         &states_ad));
  base::RunLoop().RunUntilIdle();
  EXPECT_EQ(BLACKLISTED_MALWARE, states_ad[a]);
  EXPECT_EQ(BLACKLISTED_POTENTIALLY_UNWANTED, states_ad[d]);
  EXPECT_EQ(states_ad.end(), states_ad.find(e));
  EXPECT_EQ(old_request_count, fetcher->request_count());
}

// Test both Blacklist and BlacklistStateFetcher by requesting the blacklist
// states, sending fake requests and parsing the responses.
TEST_F(BlacklistTest, FetchBlacklistStates) {
  Blacklist blacklist(prefs());

  std::string a = AddExtension("a");
  std::string b = AddExtension("b");
  std::string c = AddExtension("c");

  blacklist_db()->Enable();
  blacklist_db()->SetUnsafe(a, b);

  // Prepare real fetcher.
  BlacklistStateFetcher* fetcher = new BlacklistStateFetcher();
  TestBlacklistStateFetcher fetcher_tester(fetcher);
  blacklist.SetBlacklistStateFetcherForTest(fetcher);

  fetcher_tester.SetBlacklistVerdict(
      a, ClientCRXListInfoResponse_Verdict_CWS_POLICY_VIOLATION);
  fetcher_tester.SetBlacklistVerdict(
      b, ClientCRXListInfoResponse_Verdict_POTENTIALLY_UNWANTED);

  Blacklist::BlacklistStateMap states;
  blacklist.GetBlacklistedIDs(
      Set(a, b, c), base::Bind(&Assign<Blacklist::BlacklistStateMap>, &states));
  base::RunLoop().RunUntilIdle();

   // Two fetchers should be created.
  EXPECT_TRUE(fetcher_tester.HandleFetcher(0));
  EXPECT_TRUE(fetcher_tester.HandleFetcher(1));

  EXPECT_EQ(BLACKLISTED_CWS_POLICY_VIOLATION, states[a]);
  EXPECT_EQ(BLACKLISTED_POTENTIALLY_UNWANTED, states[b]);
  EXPECT_EQ(states.end(), states.find(c));

  Blacklist::BlacklistStateMap cached_states;

  blacklist.GetBlacklistedIDs(
      Set(a, b, c), base::Bind(&Assign<Blacklist::BlacklistStateMap>,
                               &cached_states));
  base::RunLoop().RunUntilIdle();

  // No new fetchers.
  EXPECT_FALSE(fetcher_tester.HandleFetcher(2));
  EXPECT_EQ(BLACKLISTED_CWS_POLICY_VIOLATION, cached_states[a]);
  EXPECT_EQ(BLACKLISTED_POTENTIALLY_UNWANTED, cached_states[b]);
  EXPECT_EQ(cached_states.end(), cached_states.find(c));
}

}  // namespace extensions
