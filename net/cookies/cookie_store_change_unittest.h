// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef NET_COOKIES_COOKIE_STORE_CHANGE_UNITTEST_H
#define NET_COOKIES_COOKIE_STORE_CHANGE_UNITTEST_H

#include "base/bind.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_store.h"
#include "net/cookies/cookie_store_unittest.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace net {

namespace {

using CookieChange = std::pair<CanonicalCookie, CookieChangeCause>;

void OnCookieChange(std::vector<CookieChange>* changes,
                    const CanonicalCookie& cookie,
                    CookieChangeCause cause) {
  CookieChange notification(cookie, cause);
  changes->push_back(notification);
}

}  // namespace

// Google Test supports at most 50 tests per typed case, so the tests here are
// broken up into multiple cases.
template <class CookieStoreTestTraits>
class CookieStoreChangeTestBase
    : public CookieStoreTest<CookieStoreTestTraits> {
 protected:
  using CookieStoreTest<CookieStoreTestTraits>::FindAndDeleteCookie;

  bool FindAndDeleteCookie(CookieStore* cs,
                           const std::string& domain,
                           const std::string& name,
                           const std::string& path) {
    for (auto& cookie : this->GetAllCookies(cs)) {
      if (cookie.Domain() == domain && cookie.Name() == name &&
          cookie.Path() == path) {
        return this->DeleteCanonicalCookie(cs, cookie);
      }
    }

    return false;
  }
};

template <class CookieStoreTestTraits>
class CookieStoreChangeGlobalTest
    : public CookieStoreChangeTestBase<CookieStoreTestTraits> {};
TYPED_TEST_CASE_P(CookieStoreChangeGlobalTest);

template <class CookieStoreTestTraits>
class CookieStoreChangeNamedTest
    : public CookieStoreChangeTestBase<CookieStoreTestTraits> {};
TYPED_TEST_CASE_P(CookieStoreChangeNamedTest);

TYPED_TEST_P(CookieStoreChangeGlobalTest, NoCookie) {
  if (!TypeParam::supports_global_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChange> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &OnCookieChange, base::Unretained(&cookie_changes)));
  this->RunUntilIdle();
  EXPECT_EQ(0u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, InitialCookie) {
  if (!TypeParam::supports_global_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChange> cookie_changes;
  this->SetCookie(cs, this->http_www_foo_.url(), "A=B");
  this->RunUntilIdle();
  std::unique_ptr<CookieChangeSubscription> subscription(
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &OnCookieChange, base::Unretained(&cookie_changes))));
  this->RunUntilIdle();
  EXPECT_EQ(0u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, InsertOne) {
  if (!TypeParam::supports_global_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChange> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &OnCookieChange, base::Unretained(&cookie_changes)));
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->RunUntilIdle();

  ASSERT_EQ(1u, cookie_changes.size());
  EXPECT_EQ(CookieChangeCause::INSERTED, cookie_changes[0].second);
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[0].first.Domain());
  EXPECT_EQ("A", cookie_changes[0].first.Name());
  EXPECT_EQ("B", cookie_changes[0].first.Value());
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, InsertMany) {
  if (!TypeParam::supports_global_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChange> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &OnCookieChange, base::Unretained(&cookie_changes)));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "C=D"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "E=F"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_bar_com_.url(), "G=H"));
  this->RunUntilIdle();

  // Check that the cookie changes are dispatched before calling GetCookies.
  // This is not an ASSERT because the following expectations produce useful
  // debugging information if they fail.
  EXPECT_EQ(4u, cookie_changes.size());
  EXPECT_EQ("A=B; C=D; E=F", this->GetCookies(cs, this->http_www_foo_.url()));
  EXPECT_EQ("G=H", this->GetCookies(cs, this->http_bar_com_.url()));

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ(CookieChangeCause::INSERTED, cookie_changes[0].second);
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[0].first.Domain());
  EXPECT_EQ("A", cookie_changes[0].first.Name());
  EXPECT_EQ("B", cookie_changes[0].first.Value());

  ASSERT_LE(2u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[1].first.Domain());
  EXPECT_EQ(CookieChangeCause::INSERTED, cookie_changes[1].second);
  EXPECT_EQ("C", cookie_changes[1].first.Name());
  EXPECT_EQ("D", cookie_changes[1].first.Value());

  ASSERT_LE(3u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[2].first.Domain());
  EXPECT_EQ(CookieChangeCause::INSERTED, cookie_changes[2].second);
  EXPECT_EQ("E", cookie_changes[2].first.Name());
  EXPECT_EQ("F", cookie_changes[2].first.Value());

  ASSERT_LE(4u, cookie_changes.size());
  EXPECT_EQ(this->http_bar_com_.url().host(), cookie_changes[3].first.Domain());
  EXPECT_EQ(CookieChangeCause::INSERTED, cookie_changes[3].second);
  EXPECT_EQ("G", cookie_changes[3].first.Name());
  EXPECT_EQ("H", cookie_changes[3].first.Value());

  EXPECT_EQ(4u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, DeleteOne) {
  if (!TypeParam::supports_global_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChange> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &OnCookieChange, base::Unretained(&cookie_changes)));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->RunUntilIdle();
  EXPECT_EQ(1u, cookie_changes.size());
  cookie_changes.clear();

  EXPECT_TRUE(
      this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(), "A"));
  this->RunUntilIdle();

  ASSERT_EQ(1u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[0].first.Domain());
  EXPECT_EQ(CookieChangeCause::EXPLICIT, cookie_changes[0].second);
  EXPECT_EQ("A", cookie_changes[0].first.Name());
  EXPECT_EQ("B", cookie_changes[0].first.Value());
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, DeleteTwo) {
  if (!TypeParam::supports_global_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChange> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &OnCookieChange, base::Unretained(&cookie_changes)));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "C=D"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "E=F"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_bar_com_.url(), "G=H"));
  this->RunUntilIdle();
  EXPECT_EQ(4u, cookie_changes.size());
  cookie_changes.clear();

  EXPECT_TRUE(
      this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(), "C"));
  EXPECT_TRUE(
      this->FindAndDeleteCookie(cs, this->http_bar_com_.url().host(), "G"));
  this->RunUntilIdle();

  // Check that the cookie changes are dispatched before calling GetCookies.
  // This is not an ASSERT because the following expectations produce useful
  // debugging information if they fail.
  EXPECT_EQ(2u, cookie_changes.size());
  EXPECT_EQ("A=B; E=F", this->GetCookies(cs, this->http_www_foo_.url()));
  EXPECT_EQ("", this->GetCookies(cs, this->http_bar_com_.url()));

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[0].first.Domain());
  EXPECT_EQ(CookieChangeCause::EXPLICIT, cookie_changes[0].second);
  EXPECT_EQ("C", cookie_changes[0].first.Name());
  EXPECT_EQ("D", cookie_changes[0].first.Value());

  ASSERT_EQ(2u, cookie_changes.size());
  EXPECT_EQ(this->http_bar_com_.url().host(), cookie_changes[1].first.Domain());
  EXPECT_EQ(CookieChangeCause::EXPLICIT, cookie_changes[1].second);
  EXPECT_EQ("G", cookie_changes[1].first.Name());
  EXPECT_EQ("H", cookie_changes[1].first.Value());
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, Overwrite) {
  if (!TypeParam::supports_global_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChange> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &OnCookieChange, base::Unretained(&cookie_changes)));
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->RunUntilIdle();
  ASSERT_EQ(1u, cookie_changes.size());
  cookie_changes.clear();

  // Replacing an existing cookie is actually a two-phase delete + set
  // operation, so we get an extra notification.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=C"));
  this->RunUntilIdle();

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[0].first.Domain());
  EXPECT_EQ(CookieChangeCause::OVERWRITE, cookie_changes[0].second);
  EXPECT_EQ("A", cookie_changes[0].first.Name());
  EXPECT_EQ("B", cookie_changes[0].first.Value());

  ASSERT_LE(2u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[1].first.Domain());
  EXPECT_EQ(CookieChangeCause::INSERTED, cookie_changes[1].second);
  EXPECT_EQ("A", cookie_changes[1].first.Name());
  EXPECT_EQ("C", cookie_changes[1].first.Value());

  EXPECT_EQ(2u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, OverwriteWithHttpOnly) {
  if (!TypeParam::supports_global_cookie_tracking)
    return;

  // Insert a cookie "A" for path "/path1"
  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChange> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &OnCookieChange, base::Unretained(&cookie_changes)));
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes.size());

  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "A=B; path=/path1"));
  this->RunUntilIdle();
  ASSERT_EQ(1u, cookie_changes.size());
  EXPECT_EQ(CookieChangeCause::INSERTED, cookie_changes[0].second);
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[0].first.Domain());
  EXPECT_EQ("A", cookie_changes[0].first.Name());
  EXPECT_EQ("B", cookie_changes[0].first.Value());
  cookie_changes.clear();

  // Insert a cookie "A" for path "/path1", that is httponly. This should
  // overwrite the non-http-only version.
  CookieOptions allow_httponly;
  allow_httponly.set_include_httponly();
  EXPECT_TRUE(this->SetCookieWithOptions(cs, this->http_www_foo_.url(),
                                         "A=C; path=/path1; httponly",
                                         allow_httponly));
  this->RunUntilIdle();

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[0].first.Domain());
  EXPECT_EQ(CookieChangeCause::OVERWRITE, cookie_changes[0].second);
  EXPECT_EQ("A", cookie_changes[0].first.Name());
  EXPECT_EQ("B", cookie_changes[0].first.Value());

  ASSERT_LE(2u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[1].first.Domain());
  EXPECT_EQ(CookieChangeCause::INSERTED, cookie_changes[1].second);
  EXPECT_EQ("A", cookie_changes[1].first.Name());
  EXPECT_EQ("C", cookie_changes[1].first.Value());

  EXPECT_EQ(2u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, Deregister) {
  if (!TypeParam::supports_global_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChange> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &OnCookieChange, base::Unretained(&cookie_changes)));
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes.size());

  // Insert a cookie and make sure it is seen.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->RunUntilIdle();
  ASSERT_EQ(1u, cookie_changes.size());
  EXPECT_EQ("A", cookie_changes[0].first.Name());
  EXPECT_EQ("B", cookie_changes[0].first.Value());
  cookie_changes.clear();

  // De-register the subscription.
  subscription.reset();

  // Insert a second cookie and make sure that it's not visible.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "C=D"));
  this->RunUntilIdle();

  EXPECT_EQ(0u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, DeregisterMultiple) {
  if (!TypeParam::supports_global_cookie_tracking ||
      !TypeParam::supports_multiple_tracking_callbacks)
    return;

  CookieStore* cs = this->GetCookieStore();

  // Register two subscriptions.
  std::vector<CookieChange> cookie_changes_1;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &OnCookieChange, base::Unretained(&cookie_changes_1)));

  std::vector<CookieChange> cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &OnCookieChange, base::Unretained(&cookie_changes_2)));
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());

  // Insert a cookie and make sure it's seen.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->RunUntilIdle();
  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("A", cookie_changes_1[0].first.Name());
  EXPECT_EQ("B", cookie_changes_1[0].first.Value());
  cookie_changes_1.clear();

  ASSERT_EQ(1u, cookie_changes_2.size());
  EXPECT_EQ("A", cookie_changes_2[0].first.Name());
  EXPECT_EQ("B", cookie_changes_2[0].first.Value());
  cookie_changes_2.clear();

  // De-register the second subscription.
  subscription2.reset();

  // Insert a second cookie and make sure that it's only visible in one
  // change array.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "C=D"));
  this->RunUntilIdle();
  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("C", cookie_changes_1[0].first.Name());
  EXPECT_EQ("D", cookie_changes_1[0].first.Value());
  cookie_changes_1.clear();

  ASSERT_EQ(0u, cookie_changes_2.size());
}

// Confirm that a listener does not receive notifications for changes that
// happened right before the subscription was established.
TYPED_TEST_P(CookieStoreChangeGlobalTest, DispatchRace) {
  if (!TypeParam::supports_global_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  // This cookie insertion should not be seen.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  // RunUntilIdle() must NOT be called before the subscription is established.

  std::vector<CookieChange> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &OnCookieChange, base::Unretained(&cookie_changes)));

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "C=D"));
  this->RunUntilIdle();

  EXPECT_LE(1u, cookie_changes.size());
  EXPECT_EQ("C", cookie_changes[0].first.Name());
  EXPECT_EQ("D", cookie_changes[0].first.Value());

  ASSERT_EQ(1u, cookie_changes.size());
}

// Confirm that deregistering a subscription blocks the notification if the
// deregistration happened after the change but before the notification was
// received.
TYPED_TEST_P(CookieStoreChangeGlobalTest, DeregisterRace) {
  if (!TypeParam::supports_global_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChange> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &OnCookieChange, base::Unretained(&cookie_changes)));
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes.size());

  // Insert a cookie and make sure it's seen.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->RunUntilIdle();
  ASSERT_EQ(1u, cookie_changes.size());
  EXPECT_EQ("A", cookie_changes[0].first.Name());
  EXPECT_EQ("B", cookie_changes[0].first.Value());
  cookie_changes.clear();

  // Insert a cookie, confirm it is not seen, deregister the subscription, run
  // until idle, and confirm the cookie is still not seen.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "C=D"));

  // Note that by the API contract it's perfectly valid to have received the
  // notification immediately, i.e. synchronously with the cookie change. In
  // that case, there's nothing to test.
  if (1u == cookie_changes.size())
    return;

  // A task was posted by the SetCookie() above, but has not yet arrived. If it
  // arrived before the subscription is destroyed, callback execution would be
  // valid. Destroy the subscription so as to lose the race and make sure the
  // task posted arrives after the subscription was destroyed.
  subscription.reset();
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, DeregisterRaceMultiple) {
  if (!TypeParam::supports_global_cookie_tracking ||
      !TypeParam::supports_multiple_tracking_callbacks)
    return;

  CookieStore* cs = this->GetCookieStore();

  // Register two subscriptions.
  std::vector<CookieChange> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &OnCookieChange, base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &OnCookieChange, base::Unretained(&cookie_changes_2)));
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());

  // Insert a cookie and make sure it's seen.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->RunUntilIdle();

  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("A", cookie_changes_1[0].first.Name());
  EXPECT_EQ("B", cookie_changes_1[0].first.Value());
  cookie_changes_1.clear();

  ASSERT_EQ(1u, cookie_changes_2.size());
  EXPECT_EQ("A", cookie_changes_2[0].first.Name());
  EXPECT_EQ("B", cookie_changes_2[0].first.Value());
  cookie_changes_2.clear();

  // Insert a cookie, confirm it is not seen, deregister a subscription, run
  // until idle, and confirm the cookie is still not seen.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "C=D"));

  // Note that by the API contract it's perfectly valid to have received the
  // notification immediately, i.e. synchronously with the cookie change. In
  // that case, there's nothing to test.
  if (1u == cookie_changes_2.size())
    return;

  // A task was posted by the SetCookie() above, but has not yet arrived. If it
  // arrived before the subscription is destroyed, callback execution would be
  // valid. Destroy one of the subscriptions so as to lose the race and make
  // sure the task posted arrives after the subscription was destroyed.
  subscription2.reset();
  this->RunUntilIdle();
  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("C", cookie_changes_1[0].first.Name());
  EXPECT_EQ("D", cookie_changes_1[0].first.Value());

  // No late notification was received.
  ASSERT_EQ(0u, cookie_changes_2.size());
}

TYPED_TEST_P(CookieStoreChangeGlobalTest, MultipleSubscriptions) {
  if (!TypeParam::supports_global_cookie_tracking ||
      !TypeParam::supports_multiple_tracking_callbacks)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChange> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &OnCookieChange, base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForAllChanges(base::BindRepeating(
          &OnCookieChange, base::Unretained(&cookie_changes_2)));
  this->RunUntilIdle();

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "A=B"));
  this->RunUntilIdle();

  ASSERT_EQ(1U, cookie_changes_1.size());
  EXPECT_EQ("A", cookie_changes_1[0].first.Name());
  EXPECT_EQ("B", cookie_changes_1[0].first.Value());

  ASSERT_EQ(1U, cookie_changes_2.size());
  EXPECT_EQ("A", cookie_changes_2[0].first.Name());
  EXPECT_EQ("B", cookie_changes_2[0].first.Value());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, NoCookie) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChange> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes)));
  this->RunUntilIdle();
  EXPECT_EQ(0u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, InitialCookie) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChange> cookie_changes;
  this->SetCookie(cs, this->http_www_foo_.url(), "abc=def");
  this->RunUntilIdle();
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes)));
  this->RunUntilIdle();
  EXPECT_EQ(0u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, InsertOne) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChange> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes)));
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=def"));
  this->RunUntilIdle();
  ASSERT_EQ(1u, cookie_changes.size());

  EXPECT_EQ("abc", cookie_changes[0].first.Name());
  EXPECT_EQ("def", cookie_changes[0].first.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[0].first.Domain());
  EXPECT_EQ(CookieChangeCause::INSERTED, cookie_changes[0].second);
}

TYPED_TEST_P(CookieStoreChangeNamedTest, InsertTwo) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChange> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes)));
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=def"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=hij; path=/foo"));
  this->RunUntilIdle();

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[0].first.Name());
  EXPECT_EQ("def", cookie_changes[0].first.Value());
  EXPECT_EQ("/", cookie_changes[0].first.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[0].first.Domain());
  EXPECT_EQ(CookieChangeCause::INSERTED, cookie_changes[0].second);

  ASSERT_LE(2u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[1].first.Name());
  EXPECT_EQ("hij", cookie_changes[1].first.Value());
  EXPECT_EQ("/foo", cookie_changes[1].first.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[0].first.Domain());
  EXPECT_EQ(CookieChangeCause::INSERTED, cookie_changes[0].second);

  EXPECT_EQ(2u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, InsertFiltering) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChange> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes)));
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes.size());

  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=def; path=/"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_bar_com_.url(), "abc=ghi; path=/"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=jkl; path=/bar"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=mno; path=/foo/bar"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "xyz=zyx"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=pqr; path=/foo"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(),
                              "abc=stu; domain=foo.com"));
  this->RunUntilIdle();

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[0].first.Name());
  EXPECT_EQ("def", cookie_changes[0].first.Value());
  EXPECT_EQ("/", cookie_changes[0].first.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[0].first.Domain());
  EXPECT_EQ(CookieChangeCause::INSERTED, cookie_changes[0].second);

  ASSERT_LE(2u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[1].first.Name());
  EXPECT_EQ("pqr", cookie_changes[1].first.Value());
  EXPECT_EQ("/foo", cookie_changes[1].first.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[1].first.Domain());
  EXPECT_EQ(CookieChangeCause::INSERTED, cookie_changes[1].second);

  ASSERT_LE(3u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[2].first.Name());
  EXPECT_EQ("stu", cookie_changes[2].first.Value());
  EXPECT_EQ("/", cookie_changes[2].first.Path());
  EXPECT_EQ(".foo.com", cookie_changes[2].first.Domain());
  EXPECT_EQ(CookieChangeCause::INSERTED, cookie_changes[1].second);

  EXPECT_EQ(3u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, DeleteOne) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChange> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes)));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=def"));
  this->RunUntilIdle();
  EXPECT_EQ(1u, cookie_changes.size());
  cookie_changes.clear();

  EXPECT_TRUE(
      this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(), "abc"));
  this->RunUntilIdle();

  ASSERT_EQ(1u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[0].first.Name());
  EXPECT_EQ("def", cookie_changes[0].first.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[0].first.Domain());
  EXPECT_EQ(CookieChangeCause::EXPLICIT, cookie_changes[0].second);
}

TYPED_TEST_P(CookieStoreChangeNamedTest, DeleteTwo) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChange> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes)));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=def"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=hij; path=/foo"));
  this->RunUntilIdle();
  EXPECT_EQ(2u, cookie_changes.size());
  cookie_changes.clear();

  EXPECT_TRUE(this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(),
                                        "abc", "/"));
  EXPECT_TRUE(this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(),
                                        "abc", "/foo"));
  this->RunUntilIdle();

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[0].first.Name());
  EXPECT_EQ("def", cookie_changes[0].first.Value());
  EXPECT_EQ("/", cookie_changes[0].first.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[0].first.Domain());
  EXPECT_EQ(CookieChangeCause::EXPLICIT, cookie_changes[0].second);

  ASSERT_EQ(2u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[1].first.Name());
  EXPECT_EQ("hij", cookie_changes[1].first.Value());
  EXPECT_EQ("/foo", cookie_changes[1].first.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[1].first.Domain());
  EXPECT_EQ(CookieChangeCause::EXPLICIT, cookie_changes[1].second);
}

TYPED_TEST_P(CookieStoreChangeNamedTest, DeleteFiltering) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChange> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes)));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "xyz=zyx; path=/"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_bar_com_.url(), "abc=def; path=/"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=hij; path=/foo/bar"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=mno; path=/foo"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=pqr; path=/"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(),
                              "abc=stu; domain=foo.com"));
  this->RunUntilIdle();
  EXPECT_EQ(3u, cookie_changes.size());
  cookie_changes.clear();

  EXPECT_TRUE(
      this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(), "xyz"));
  EXPECT_TRUE(
      this->FindAndDeleteCookie(cs, this->http_bar_com_.url().host(), "abc"));
  EXPECT_TRUE(this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(),
                                        "abc", "/foo/bar"));
  EXPECT_TRUE(this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(),
                                        "abc", "/foo"));
  EXPECT_TRUE(this->FindAndDeleteCookie(cs, this->http_www_foo_.url().host(),
                                        "abc", "/"));
  EXPECT_TRUE(this->FindAndDeleteCookie(cs, ".foo.com", "abc", "/"));
  this->RunUntilIdle();

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[0].first.Name());
  EXPECT_EQ("mno", cookie_changes[0].first.Value());
  EXPECT_EQ("/foo", cookie_changes[0].first.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[0].first.Domain());
  EXPECT_EQ(CookieChangeCause::EXPLICIT, cookie_changes[0].second);

  ASSERT_LE(2u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[1].first.Name());
  EXPECT_EQ("pqr", cookie_changes[1].first.Value());
  EXPECT_EQ("/", cookie_changes[1].first.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[1].first.Domain());
  EXPECT_EQ(CookieChangeCause::EXPLICIT, cookie_changes[1].second);

  ASSERT_LE(3u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[2].first.Name());
  EXPECT_EQ("stu", cookie_changes[2].first.Value());
  EXPECT_EQ("/", cookie_changes[2].first.Path());
  EXPECT_EQ(".foo.com", cookie_changes[2].first.Domain());
  EXPECT_EQ(CookieChangeCause::EXPLICIT, cookie_changes[2].second);

  EXPECT_EQ(3u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, Overwrite) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChange> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes)));
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=def"));
  this->RunUntilIdle();
  EXPECT_EQ(1u, cookie_changes.size());
  cookie_changes.clear();

  // Replacing an existing cookie is actually a two-phase delete + set
  // operation, so we get an extra notification.
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=ghi"));
  this->RunUntilIdle();

  EXPECT_LE(1u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[0].first.Name());
  EXPECT_EQ("def", cookie_changes[0].first.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[0].first.Domain());
  EXPECT_EQ(CookieChangeCause::OVERWRITE, cookie_changes[0].second);

  EXPECT_LE(2u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[1].first.Name());
  EXPECT_EQ("ghi", cookie_changes[1].first.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[1].first.Domain());
  EXPECT_EQ(CookieChangeCause::INSERTED, cookie_changes[1].second);

  EXPECT_EQ(2u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, OverwriteFiltering) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChange> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes)));
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes.size());

  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "xyz=zyx; path=/"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_bar_com_.url(), "abc=def; path=/"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=hij; path=/foo/bar"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=mno; path=/foo"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=pqr; path=/"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(),
                              "abc=stu; domain=foo.com"));
  this->RunUntilIdle();
  EXPECT_EQ(3u, cookie_changes.size());
  cookie_changes.clear();

  // Replacing an existing cookie is actually a two-phase delete + set
  // operation, so we get two notifications per overwrite.
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "xyz=xyz; path=/"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_bar_com_.url(), "abc=DEF; path=/"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=HIJ; path=/foo/bar"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=MNO; path=/foo"));
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=PQR; path=/"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(),
                              "abc=STU; domain=foo.com"));
  this->RunUntilIdle();

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[0].first.Name());
  EXPECT_EQ("mno", cookie_changes[0].first.Value());
  EXPECT_EQ("/foo", cookie_changes[0].first.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[0].first.Domain());
  EXPECT_EQ(CookieChangeCause::OVERWRITE, cookie_changes[0].second);

  ASSERT_LE(2u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[1].first.Name());
  EXPECT_EQ("MNO", cookie_changes[1].first.Value());
  EXPECT_EQ("/foo", cookie_changes[1].first.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[1].first.Domain());
  EXPECT_EQ(CookieChangeCause::INSERTED, cookie_changes[1].second);

  ASSERT_LE(3u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[2].first.Name());
  EXPECT_EQ("pqr", cookie_changes[2].first.Value());
  EXPECT_EQ("/", cookie_changes[2].first.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[2].first.Domain());
  EXPECT_EQ(CookieChangeCause::OVERWRITE, cookie_changes[2].second);

  ASSERT_LE(4u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[3].first.Name());
  EXPECT_EQ("PQR", cookie_changes[3].first.Value());
  EXPECT_EQ("/", cookie_changes[3].first.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[3].first.Domain());
  EXPECT_EQ(CookieChangeCause::INSERTED, cookie_changes[3].second);

  ASSERT_LE(5u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[4].first.Name());
  EXPECT_EQ("stu", cookie_changes[4].first.Value());
  EXPECT_EQ("/", cookie_changes[4].first.Path());
  EXPECT_EQ(".foo.com", cookie_changes[4].first.Domain());
  EXPECT_EQ(CookieChangeCause::OVERWRITE, cookie_changes[4].second);

  ASSERT_LE(6u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[5].first.Name());
  EXPECT_EQ("STU", cookie_changes[5].first.Value());
  EXPECT_EQ("/", cookie_changes[5].first.Path());
  EXPECT_EQ(".foo.com", cookie_changes[5].first.Domain());
  EXPECT_EQ(CookieChangeCause::INSERTED, cookie_changes[5].second);

  EXPECT_EQ(6u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, OverwriteWithHttpOnly) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  // Insert a cookie "abc" for path "/foo"
  CookieStore* cs = this->GetCookieStore();
  std::vector<CookieChange> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes)));
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes.size());

  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=def; path=/foo"));
  this->RunUntilIdle();
  ASSERT_EQ(1u, cookie_changes.size());
  EXPECT_EQ(CookieChangeCause::INSERTED, cookie_changes[0].second);
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[0].first.Domain());
  EXPECT_EQ("abc", cookie_changes[0].first.Name());
  EXPECT_EQ("def", cookie_changes[0].first.Value());
  cookie_changes.clear();

  // Insert a cookie "a" for path "/path1", that is httponly. This should
  // overwrite the non-http-only version.
  CookieOptions allow_httponly;
  allow_httponly.set_include_httponly();
  EXPECT_TRUE(this->SetCookieWithOptions(cs, this->http_www_foo_.url(),
                                         "abc=hij; path=/foo; httponly",
                                         allow_httponly));
  this->RunUntilIdle();

  ASSERT_LE(1u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[0].first.Domain());
  EXPECT_EQ(CookieChangeCause::OVERWRITE, cookie_changes[0].second);
  EXPECT_EQ("abc", cookie_changes[0].first.Name());
  EXPECT_EQ("def", cookie_changes[0].first.Value());

  ASSERT_LE(2u, cookie_changes.size());
  EXPECT_EQ(this->http_www_foo_.url().host(), cookie_changes[1].first.Domain());
  EXPECT_EQ(CookieChangeCause::INSERTED, cookie_changes[1].second);
  EXPECT_EQ("abc", cookie_changes[1].first.Name());
  EXPECT_EQ("hij", cookie_changes[1].first.Value());

  EXPECT_EQ(2u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, Deregister) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChange> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes)));
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes.size());

  // Insert a cookie and make sure it is seen.
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=def; path=/foo"));
  this->RunUntilIdle();
  ASSERT_EQ(1u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[0].first.Name());
  EXPECT_EQ("def", cookie_changes[0].first.Value());
  EXPECT_EQ("/foo", cookie_changes[0].first.Path());
  cookie_changes.clear();

  // De-register the subscription.
  subscription.reset();

  // Insert a second cookie and make sure it's not visible.
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=hij; path=/"));
  this->RunUntilIdle();

  EXPECT_EQ(0u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, DeregisterMultiple) {
  if (!TypeParam::supports_named_cookie_tracking ||
      !TypeParam::supports_multiple_tracking_callbacks)
    return;

  CookieStore* cs = this->GetCookieStore();

  // Register two subscriptions.
  std::vector<CookieChange> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes_2)));
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());

  // Insert a cookie and make sure it's seen.
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=def; path=/foo"));
  this->RunUntilIdle();
  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("abc", cookie_changes_1[0].first.Name());
  EXPECT_EQ("def", cookie_changes_1[0].first.Value());
  EXPECT_EQ("/foo", cookie_changes_1[0].first.Path());
  cookie_changes_1.clear();

  ASSERT_EQ(1u, cookie_changes_2.size());
  EXPECT_EQ("abc", cookie_changes_2[0].first.Name());
  EXPECT_EQ("def", cookie_changes_2[0].first.Value());
  EXPECT_EQ("/foo", cookie_changes_2[0].first.Path());
  cookie_changes_2.clear();

  // De-register the second registration.
  subscription2.reset();

  // Insert a second cookie and make sure that it's only visible in one
  // change array.
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=hij; path=/"));
  this->RunUntilIdle();
  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("abc", cookie_changes_1[0].first.Name());
  EXPECT_EQ("hij", cookie_changes_1[0].first.Value());
  EXPECT_EQ("/", cookie_changes_1[0].first.Path());

  EXPECT_EQ(0u, cookie_changes_2.size());
}

// Confirm that a listener does not receive notifications for changes that
// happened right before the subscription was established.
TYPED_TEST_P(CookieStoreChangeNamedTest, DispatchRace) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  // This cookie insertion should not be seen.
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=def; path=/foo"));
  // RunUntilIdle() must NOT be called before the subscription is established.

  std::vector<CookieChange> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes)));

  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=hij; path=/"));
  this->RunUntilIdle();

  EXPECT_LE(1u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[0].first.Name());
  EXPECT_EQ("hij", cookie_changes[0].first.Value());
  EXPECT_EQ("/", cookie_changes[0].first.Path());

  ASSERT_EQ(1u, cookie_changes.size());
}

// Confirm that deregistering a subscription blocks the notification if the
// deregistration happened after the change but before the notification was
// received.
TYPED_TEST_P(CookieStoreChangeNamedTest, DeregisterRace) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChange> cookie_changes;
  std::unique_ptr<CookieChangeSubscription> subscription =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes)));
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes.size());

  // Insert a cookie and make sure it's seen.
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=def; path=/foo"));
  this->RunUntilIdle();
  ASSERT_EQ(1u, cookie_changes.size());
  EXPECT_EQ("abc", cookie_changes[0].first.Name());
  EXPECT_EQ("def", cookie_changes[0].first.Value());
  EXPECT_EQ("/foo", cookie_changes[0].first.Path());
  cookie_changes.clear();

  // Insert a cookie, confirm it is not seen, deregister the subscription, run
  // until idle, and confirm the cookie is still not seen.
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=hij; path=/"));

  // Note that by the API contract it's perfectly valid to have received the
  // notification immediately, i.e. synchronously with the cookie change. In
  // that case, there's nothing to test.
  if (1u == cookie_changes.size())
    return;

  // A task was posted by the SetCookie() above, but has not yet arrived. If it
  // arrived before the subscription is destroyed, callback execution would be
  // valid. Destroy the subscription so as to lose the race and make sure the
  // task posted arrives after the subscription was destroyed.
  subscription.reset();
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, DeregisterRaceMultiple) {
  if (!TypeParam::supports_named_cookie_tracking ||
      !TypeParam::supports_multiple_tracking_callbacks)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChange> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes_2)));
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());

  // Insert a cookie and make sure it's seen.
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=def; path=/foo"));
  this->RunUntilIdle();

  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("abc", cookie_changes_1[0].first.Name());
  EXPECT_EQ("def", cookie_changes_1[0].first.Value());
  EXPECT_EQ("/foo", cookie_changes_1[0].first.Path());
  cookie_changes_1.clear();

  ASSERT_EQ(1u, cookie_changes_2.size());
  EXPECT_EQ("abc", cookie_changes_2[0].first.Name());
  EXPECT_EQ("def", cookie_changes_2[0].first.Value());
  EXPECT_EQ("/foo", cookie_changes_2[0].first.Path());
  cookie_changes_2.clear();

  // Insert a cookie, confirm it is not seen, deregister a subscription, run
  // until idle, and confirm the cookie is still not seen.
  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=hij; path=/"));

  // Note that by the API contract it's perfectly valid to have received the
  // notification immediately, i.e. synchronously with the cookie change. In
  // that case, there's nothing to test.
  if (1u == cookie_changes_2.size())
    return;

  // A task was posted by the SetCookie() above, but has not yet arrived. If it
  // arrived before the subscription is destroyed, callback execution would be
  // valid. Destroy one of the subscriptions so as to lose the race and make
  // sure the task posted arrives after the subscription was destroyed.
  subscription2.reset();
  this->RunUntilIdle();
  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("abc", cookie_changes_1[0].first.Name());
  EXPECT_EQ("hij", cookie_changes_1[0].first.Value());
  EXPECT_EQ("/", cookie_changes_1[0].first.Path());

  // No late notification was received.
  ASSERT_EQ(0u, cookie_changes_2.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, DifferentSubscriptionsDisjoint) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChange> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_bar_com_.url(), "ghi",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes_2)));
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=def"));
  this->RunUntilIdle();
  EXPECT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ(0u, cookie_changes_2.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_bar_com_.url(), "ghi=jkl"));
  this->RunUntilIdle();

  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("abc", cookie_changes_1[0].first.Name());
  EXPECT_EQ("def", cookie_changes_1[0].first.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_1[0].first.Domain());

  ASSERT_EQ(1u, cookie_changes_2.size());
  EXPECT_EQ("ghi", cookie_changes_2[0].first.Name());
  EXPECT_EQ("jkl", cookie_changes_2[0].first.Value());
  EXPECT_EQ(this->http_bar_com_.url().host(),
            cookie_changes_2[0].first.Domain());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, DifferentSubscriptionsDomains) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChange> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_bar_com_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes_2)));
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=def"));
  this->RunUntilIdle();
  EXPECT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ(0u, cookie_changes_2.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_bar_com_.url(), "abc=ghi"));
  this->RunUntilIdle();

  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("abc", cookie_changes_1[0].first.Name());
  EXPECT_EQ("def", cookie_changes_1[0].first.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_1[0].first.Domain());

  ASSERT_EQ(1u, cookie_changes_2.size());
  EXPECT_EQ("abc", cookie_changes_2[0].first.Name());
  EXPECT_EQ("ghi", cookie_changes_2[0].first.Value());
  EXPECT_EQ(this->http_bar_com_.url().host(),
            cookie_changes_2[0].first.Domain());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, DifferentSubscriptionsNames) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChange> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "ghi",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes_2)));
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=def"));
  this->RunUntilIdle();
  EXPECT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ(0u, cookie_changes_2.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "ghi=jkl"));
  this->RunUntilIdle();

  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("abc", cookie_changes_1[0].first.Name());
  EXPECT_EQ("def", cookie_changes_1[0].first.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_1[0].first.Domain());

  ASSERT_EQ(1u, cookie_changes_2.size());
  EXPECT_EQ("ghi", cookie_changes_2[0].first.Name());
  EXPECT_EQ("jkl", cookie_changes_2[0].first.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_2[0].first.Domain());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, DifferentSubscriptionsPaths) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChange> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes_2)));
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=def"));
  this->RunUntilIdle();
  EXPECT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ(1u, cookie_changes_2.size());

  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=ghi; path=/foo"));
  this->RunUntilIdle();

  ASSERT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ("abc", cookie_changes_1[0].first.Name());
  EXPECT_EQ("def", cookie_changes_1[0].first.Value());
  EXPECT_EQ("/", cookie_changes_1[0].first.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_1[0].first.Domain());

  ASSERT_LE(1u, cookie_changes_2.size());
  EXPECT_EQ("abc", cookie_changes_2[0].first.Name());
  EXPECT_EQ("def", cookie_changes_2[0].first.Value());
  EXPECT_EQ("/", cookie_changes_2[0].first.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_2[0].first.Domain());

  ASSERT_LE(2u, cookie_changes_2.size());
  EXPECT_EQ("abc", cookie_changes_2[1].first.Name());
  EXPECT_EQ("ghi", cookie_changes_2[1].first.Value());
  EXPECT_EQ("/foo", cookie_changes_2[1].first.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_2[1].first.Domain());

  EXPECT_EQ(2u, cookie_changes_2.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, DifferentSubscriptionsFiltering) {
  if (!TypeParam::supports_named_cookie_tracking)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChange> cookie_changes_1, cookie_changes_2;
  std::vector<CookieChange> cookie_changes_3, cookie_changes_4;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "hij",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes_2)));
  std::unique_ptr<CookieChangeSubscription> subscription3 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_bar_com_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes_3)));
  std::unique_ptr<CookieChangeSubscription> subscription4 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->www_foo_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes_4)));
  this->RunUntilIdle();
  ASSERT_EQ(0u, cookie_changes_1.size());
  ASSERT_EQ(0u, cookie_changes_2.size());
  EXPECT_EQ(0u, cookie_changes_3.size());
  EXPECT_EQ(0u, cookie_changes_4.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=def"));
  this->RunUntilIdle();
  EXPECT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ(0u, cookie_changes_2.size());
  EXPECT_EQ(0u, cookie_changes_3.size());
  EXPECT_EQ(1u, cookie_changes_4.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "xyz=zyx"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "hij=mno"));
  this->RunUntilIdle();
  EXPECT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ(1u, cookie_changes_2.size());
  EXPECT_EQ(0u, cookie_changes_3.size());
  EXPECT_EQ(1u, cookie_changes_4.size());

  EXPECT_TRUE(this->SetCookie(cs, this->http_bar_com_.url(), "hij=pqr"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_bar_com_.url(), "xyz=zyx"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_bar_com_.url(), "abc=stu"));
  this->RunUntilIdle();
  EXPECT_EQ(1u, cookie_changes_1.size());
  EXPECT_EQ(1u, cookie_changes_2.size());
  EXPECT_EQ(1u, cookie_changes_3.size());
  EXPECT_EQ(1u, cookie_changes_4.size());

  EXPECT_TRUE(
      this->SetCookie(cs, this->http_www_foo_.url(), "abc=vwx; path=/foo"));
  this->RunUntilIdle();

  ASSERT_LE(1u, cookie_changes_1.size());
  EXPECT_EQ("abc", cookie_changes_1[0].first.Name());
  EXPECT_EQ("def", cookie_changes_1[0].first.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_1[0].first.Domain());
  EXPECT_EQ(1u, cookie_changes_1.size());

  ASSERT_LE(1u, cookie_changes_2.size());
  EXPECT_EQ("hij", cookie_changes_2[0].first.Name());
  EXPECT_EQ("mno", cookie_changes_2[0].first.Value());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_2[0].first.Domain());
  EXPECT_EQ(1u, cookie_changes_2.size());

  ASSERT_LE(1u, cookie_changes_3.size());
  EXPECT_EQ("abc", cookie_changes_3[0].first.Name());
  EXPECT_EQ("stu", cookie_changes_3[0].first.Value());
  EXPECT_EQ(this->http_bar_com_.url().host(),
            cookie_changes_3[0].first.Domain());
  EXPECT_EQ(1u, cookie_changes_3.size());

  ASSERT_LE(1u, cookie_changes_4.size());
  EXPECT_EQ("abc", cookie_changes_4[0].first.Name());
  EXPECT_EQ("def", cookie_changes_4[0].first.Value());
  EXPECT_EQ("/", cookie_changes_4[0].first.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_4[0].first.Domain());

  ASSERT_LE(2u, cookie_changes_4.size());
  EXPECT_EQ("abc", cookie_changes_4[1].first.Name());
  EXPECT_EQ("vwx", cookie_changes_4[1].first.Value());
  EXPECT_EQ("/foo", cookie_changes_4[1].first.Path());
  EXPECT_EQ(this->http_www_foo_.url().host(),
            cookie_changes_4[1].first.Domain());

  EXPECT_EQ(2u, cookie_changes_4.size());
}

TYPED_TEST_P(CookieStoreChangeNamedTest, MultipleSubscriptions) {
  if (!TypeParam::supports_named_cookie_tracking ||
      !TypeParam::supports_multiple_tracking_callbacks)
    return;

  CookieStore* cs = this->GetCookieStore();

  std::vector<CookieChange> cookie_changes_1, cookie_changes_2;
  std::unique_ptr<CookieChangeSubscription> subscription1 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes_1)));
  std::unique_ptr<CookieChangeSubscription> subscription2 =
      cs->GetChangeDispatcher().AddCallbackForCookie(
          this->http_www_foo_.url(), "abc",
          base::BindRepeating(&OnCookieChange,
                              base::Unretained(&cookie_changes_2)));
  this->RunUntilIdle();

  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "xyz=zyx"));
  EXPECT_TRUE(this->SetCookie(cs, this->http_www_foo_.url(), "abc=def"));
  this->RunUntilIdle();

  ASSERT_EQ(1U, cookie_changes_1.size());
  EXPECT_EQ("abc", cookie_changes_1[0].first.Name());
  EXPECT_EQ("def", cookie_changes_1[0].first.Value());
  cookie_changes_1.clear();

  ASSERT_EQ(1U, cookie_changes_2.size());
  EXPECT_EQ("abc", cookie_changes_2[0].first.Name());
  EXPECT_EQ("def", cookie_changes_2[0].first.Value());
  cookie_changes_2.clear();
}

REGISTER_TYPED_TEST_CASE_P(CookieStoreChangeGlobalTest,
                           NoCookie,
                           InitialCookie,
                           InsertOne,
                           InsertMany,
                           DeleteOne,
                           DeleteTwo,
                           Overwrite,
                           OverwriteWithHttpOnly,
                           Deregister,
                           DeregisterMultiple,
                           DispatchRace,
                           DeregisterRace,
                           DeregisterRaceMultiple,
                           MultipleSubscriptions);

REGISTER_TYPED_TEST_CASE_P(CookieStoreChangeNamedTest,
                           NoCookie,
                           InitialCookie,
                           InsertOne,
                           InsertTwo,
                           InsertFiltering,
                           DeleteOne,
                           DeleteTwo,
                           DeleteFiltering,
                           Overwrite,
                           OverwriteFiltering,
                           OverwriteWithHttpOnly,
                           Deregister,
                           DeregisterMultiple,
                           DispatchRace,
                           DeregisterRace,
                           DeregisterRaceMultiple,
                           DifferentSubscriptionsDisjoint,
                           DifferentSubscriptionsDomains,
                           DifferentSubscriptionsNames,
                           DifferentSubscriptionsPaths,
                           DifferentSubscriptionsFiltering,
                           MultipleSubscriptions);

}  // namespace net

#endif  // NET_COOKIES_COOKIE_STORE_CHANGE_UNITTEST_H
