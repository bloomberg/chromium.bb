// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/strings/string_number_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/net/evicted_domain_cookie_counter.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_monster.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chrome_browser_net {

using base::Time;
using base::TimeDelta;

namespace {

const char* google_url1 = "http://www.google.com";
const char* google_url2 = "http://mail.google.com";
const char* other_url1 = "http://www.example.com";
const char* other_url2 = "http://www.example.co.uk";

class EvictedDomainCookieCounterTest : public testing::Test {
 protected:
  class MockDelegate : public EvictedDomainCookieCounter::Delegate {
   public:
    explicit MockDelegate(EvictedDomainCookieCounterTest* tester);

    // EvictedDomainCookieCounter::Delegate implementation.
    virtual void Report(
        const EvictedDomainCookieCounter::EvictedCookie& evicted_cookie,
        const Time& reinstatement_time) OVERRIDE;
    virtual Time CurrentTime() const OVERRIDE;

   private:
    EvictedDomainCookieCounterTest* tester_;
  };

  EvictedDomainCookieCounterTest();
  virtual ~EvictedDomainCookieCounterTest();

  // testing::Test implementation.
  virtual void SetUp() OVERRIDE;
  virtual void TearDown() OVERRIDE;

  // Initialization that allows parameters to be specified.
  void InitCounter(size_t max_size, size_t purge_count);

  // Wrapper to allocate new cookie and store it in |cookies_|.
  // If |max_age| == 0, then the cookie does not expire.
  void CreateNewCookie(
      const char* url, const std::string& cookie_line, int64 max_age);

  // Clears |cookies_| and creates common cookies for multiple tests.
  void InitStockCookies();

  // Sets simulation time to |rel_time|.
  void GotoTime(int64 rel_time);

  // Simulates time-passage by |delta_second|.
  void StepTime(int64 delta_second);

  // Simulates cookie addition or update.
  void Add(net::CanonicalCookie* cookie);

  // Simulates cookie removal.
  void Remove(net::CanonicalCookie* cookie);

  // Simulates cookie eviction.
  void Evict(net::CanonicalCookie* cookie);

  // For semi-realism, time considered are relative to |mock_time_base_|.
  Time mock_time_base_;
  Time mock_time_;

  // To store allocated cookies for reuse.
  ScopedVector<net::CanonicalCookie> cookies_;

  scoped_refptr<EvictedDomainCookieCounter> cookie_counter_;

  // Statistics as comma-separated string of duration (in seconds) between
  // eviction and reinstatement for each cookie, in the order of eviction.
  std::string google_stat_;
  std::string other_stat_;
};

EvictedDomainCookieCounterTest::MockDelegate::MockDelegate(
    EvictedDomainCookieCounterTest* tester)
    : tester_(tester) {}

void EvictedDomainCookieCounterTest::MockDelegate::Report(
    const EvictedDomainCookieCounter::EvictedCookie& evicted_cookie,
    const Time& reinstatement_time) {
  std::string& dest = evicted_cookie.is_google ?
      tester_->google_stat_ : tester_->other_stat_;
  if (!dest.empty())
    dest.append(",");
  TimeDelta delta(reinstatement_time - evicted_cookie.eviction_time);
  dest.append(base::Int64ToString(delta.InSeconds()));
}

Time EvictedDomainCookieCounterTest::MockDelegate::CurrentTime() const {
  return tester_->mock_time_;
}

EvictedDomainCookieCounterTest::EvictedDomainCookieCounterTest() {}

EvictedDomainCookieCounterTest::~EvictedDomainCookieCounterTest() {}

void EvictedDomainCookieCounterTest::SetUp() {
  mock_time_base_ = Time::Now() - TimeDelta::FromHours(1);
  mock_time_ = mock_time_base_;
}

void EvictedDomainCookieCounterTest::TearDown() {
}

void EvictedDomainCookieCounterTest::InitCounter(size_t max_size,
                                                 size_t purge_count) {
  scoped_ptr<MockDelegate> cookie_counter_delegate(new MockDelegate(this));
  cookie_counter_ = new EvictedDomainCookieCounter(
      NULL,
      cookie_counter_delegate.PassAs<EvictedDomainCookieCounter::Delegate>(),
      max_size,
      purge_count);
}

void EvictedDomainCookieCounterTest::CreateNewCookie(
    const char* url, const std::string& cookie_line, int64 max_age) {
  std::string line(cookie_line);
  if (max_age)
    line.append(";max-age=" + base::Int64ToString(max_age));
  net::CanonicalCookie* cookie = net::CanonicalCookie::Create(
      GURL(url), line, mock_time_, net::CookieOptions());
  DCHECK(cookie);
  cookies_.push_back(cookie);
}

void EvictedDomainCookieCounterTest::InitStockCookies() {
  cookies_.clear();
  CreateNewCookie(google_url1, "a1=1", 3000);           // cookies_[0].
  CreateNewCookie(google_url2, "a2=1", 2000);           // cookies_[1].
  CreateNewCookie(other_url1, "a1=1", 1000);            // cookies_[2].
  CreateNewCookie(other_url1, "a2=1", 1001);            // cookies_[3].
  CreateNewCookie(google_url1, "a1=1;Path=/sub", 999);  // cookies_[4].
  CreateNewCookie(other_url2, "a2=1", 0);               // cookies_[5].
}

void EvictedDomainCookieCounterTest::GotoTime(int64 rel_time) {
  mock_time_ = mock_time_base_ + TimeDelta::FromSeconds(rel_time);
}

void EvictedDomainCookieCounterTest::StepTime(int64 delta_second) {
  mock_time_ += TimeDelta::FromSeconds(delta_second);
}

void EvictedDomainCookieCounterTest::Add(net::CanonicalCookie* cookie) {
  cookie_counter_->OnCookieChanged(
      *cookie, false, net::CookieMonster::Delegate::CHANGE_COOKIE_EXPLICIT);
}

void EvictedDomainCookieCounterTest::Remove(net::CanonicalCookie* cookie) {
  cookie_counter_->OnCookieChanged(
      *cookie, true, net::CookieMonster::Delegate::CHANGE_COOKIE_EXPLICIT);
}

void EvictedDomainCookieCounterTest::Evict(net::CanonicalCookie* cookie) {
  cookie_counter_->OnCookieChanged(
      *cookie, true, net::CookieMonster::Delegate::CHANGE_COOKIE_EVICTED);
}

// EvictedDomainCookieCounter takes (and owns) a CookieMonster::Delegate for
// chaining. To ensure that the chaining indeed occurs, we implement a
// dummy CookieMonster::Delegate to increment an integer.
TEST_F(EvictedDomainCookieCounterTest, TestChain) {
  int result = 0;

  class ChangedDelegateDummy : public net::CookieMonster::Delegate {
   public:
    explicit ChangedDelegateDummy(int* result) : result_(result) {}

    virtual void OnCookieChanged(const net::CanonicalCookie& cookie,
                                 bool removed,
                                 ChangeCause cause) OVERRIDE {
      ++(*result_);
    }

    virtual void OnLoaded() OVERRIDE {}

   private:
    virtual ~ChangedDelegateDummy() {}

    int* result_;
  };

  scoped_ptr<MockDelegate> cookie_counter_delegate(new MockDelegate(this));
  cookie_counter_ = new EvictedDomainCookieCounter(
      new ChangedDelegateDummy(&result),
      cookie_counter_delegate.PassAs<EvictedDomainCookieCounter::Delegate>(),
      10,
      5);
  InitStockCookies();
  // Perform 6 cookie transactions.
  for (int i = 0; i < 6; ++i) {
    Add(cookies_[i]);
    StepTime(1);
    Evict(cookies_[i]);
    StepTime(1);
    Remove(cookies_[i]);
  }
  EXPECT_EQ(18, result);  // 6 cookies x 3 operations each.
}

// Basic flow: add cookies, evict, then reinstate.
TEST_F(EvictedDomainCookieCounterTest, TestBasicFlow) {
  InitCounter(10, 4);
  InitStockCookies();
  // Add all cookies at (relative time) t = 0.
  for (int i = 0; i < 6; ++i)
    Add(cookies_[i]);
  EXPECT_EQ(0u, cookie_counter_->GetStorageSize());  // No activities on add.
  EXPECT_EQ(";", google_stat_ + ";" + other_stat_);
  // Evict cookies at t = [1,3,6,10,15,21].
  for (int i = 0; i < 6; ++i) {
    StepTime(i + 1);
    Evict(cookies_[i]);
  }
  EXPECT_EQ(6u, cookie_counter_->GetStorageSize());  // Storing all evictions.
  EXPECT_EQ(";", google_stat_ + ";" + other_stat_);
  // Reinstate cookies at t = [22,23,24,25,26,27].
  for (int i = 0; i < 6; ++i) {
    StepTime(1);
    Add(cookies_[i]);
  }
  EXPECT_EQ(0u, cookie_counter_->GetStorageSize());  // Everything is removed.
  // Expected reinstatement delays: [21,20,18,15,11,6].
  EXPECT_EQ("21,20,11;18,15,6", google_stat_ + ";" + other_stat_);
}

// Removed cookies are ignored by EvictedDomainCookieCounter.
TEST_F(EvictedDomainCookieCounterTest, TestRemove) {
  InitCounter(10, 4);
  InitStockCookies();
  // Add all cookies at (relative time) t = 0.
  for (int i = 0; i < 6; ++i)
    Add(cookies_[i]);
  // Remove cookies at t = [1,3,6,10,15,21].
  for (int i = 0; i < 6; ++i) {
    StepTime(i + 1);
    Remove(cookies_[i]);
  }
  EXPECT_EQ(0u, cookie_counter_->GetStorageSize());
  // Add cookies again at t = [22,23,24,25,26,27].
  for (int i = 0; i < 5; ++i) {
    StepTime(1);
    Add(cookies_[i]);
  }
  EXPECT_EQ(0u, cookie_counter_->GetStorageSize());
  // No cookies were evicted, so no reinstatement take place.
  EXPECT_EQ(";", google_stat_ + ";" + other_stat_);
}

// Expired cookies should not be counted by EvictedDomainCookieCounter.
TEST_F(EvictedDomainCookieCounterTest, TestExpired) {
  InitCounter(10, 4);
  InitStockCookies();
  // Add all cookies at (relative time) t = 0.
  for (int i = 0; i < 6; ++i)
    Add(cookies_[i]);
  // Evict cookies at t = [1,3,6,10,15,21].
  for (int i = 0; i < 6; ++i) {
    StepTime(i + 1);
    Evict(cookies_[i]);
  }
  EXPECT_EQ(6u, cookie_counter_->GetStorageSize());
  GotoTime(1000);  // t = 1000, so cookies_[2,4] expire.

  // Reinstate cookies at t = [1000,1000,(1000),1000,(1000),1000].
  InitStockCookies();  // Refresh cookies, so new cookies expire in the future.
  for (int i = 0; i < 6; ++i)
    Add(cookies_[i]);
  EXPECT_EQ(0u, cookie_counter_->GetStorageSize());
  // Reinstatement delays: [999,997,(994),990,(985),979].
  EXPECT_EQ("999,997;990,979", google_stat_ + ";" + other_stat_);
}

// Garbage collection should remove the oldest evicted cookies.
TEST_F(EvictedDomainCookieCounterTest, TestGarbageCollection) {
  InitCounter(4, 2);  // Reduced capacity.
  InitStockCookies();
  // Add all cookies at (relative time) t = 0.
  for (int i = 0; i < 6; ++i)
    Add(cookies_[i]);
  // Evict cookies at t = [1,3,6,10].
  for (int i = 0; i < 4; ++i) {
    StepTime(i + 1);
    Evict(cookies_[i]);
  }
  EXPECT_EQ(4u, cookie_counter_->GetStorageSize());  // Reached capacity.
  StepTime(5);
  Evict(cookies_[4]);  // Evict at t = 15, garbage collection takes place.
  EXPECT_EQ(2u, cookie_counter_->GetStorageSize());
  StepTime(6);
  Evict(cookies_[5]);  // Evict at t = 21.
  EXPECT_EQ(3u, cookie_counter_->GetStorageSize());
  EXPECT_EQ(";", google_stat_ + ";" + other_stat_);
  // Reinstate cookies at t = [(100),(100),(100),100,100,100].
  GotoTime(100);
  for (int i = 0; i < 6; ++i)
    Add(cookies_[i]);
  // Expected reinstatement delays: [(99),(97),(94),90,85,79]
  EXPECT_EQ("85;90,79", google_stat_ + ";" + other_stat_);
}

// Garbage collection should remove the specified number of evicted cookies
// even when there are ties amongst oldest evicted cookies.
TEST_F(EvictedDomainCookieCounterTest, TestGarbageCollectionTie) {
  InitCounter(9, 3);
  // Add 10 cookies at time [0,1,3,6,...,45]
  for (int i = 0; i < 10; ++i) {
    StepTime(i);
    CreateNewCookie(google_url1, "a" + base::IntToString(i) + "=1", 3000);
    Add(cookies_[i]);
  }
  // Evict 6 cookies at t = [100,...,100].
  GotoTime(100);
  for (int i = 0; i < 6; ++i)
    Evict(cookies_[i]);
  EXPECT_EQ(6u, cookie_counter_->GetStorageSize());
  // Evict 3 cookies at t = [210,220,230].
  GotoTime(200);
  for (int i = 6; i < 9; ++i) {
    StepTime(10);
    Evict(cookies_[i]);
  }
  EXPECT_EQ(9u, cookie_counter_->GetStorageSize());  // Reached capacity.
  // Evict 1 cookie at t = 300, and garbage collection takes place.
  GotoTime(300);
  Evict(cookies_[9]);
  // Some arbitrary 4 out of 6 cookies evicted at t = 100 are gone from storage.
  EXPECT_EQ(6u, cookie_counter_->GetStorageSize());  // 10 - 4.
  // Reinstate cookies at t = [400,...,400].
  GotoTime(400);
  for (int i = 0; i < 10; ++i)
    Add(cookies_[i]);
  EXPECT_EQ(0u, cookie_counter_->GetStorageSize());
  // Expected reinstatement delays:
  // [300,300,300,300,300,300 <= keeping 2 only,190,180,170,100].
  EXPECT_EQ("300,300,190,180,170,100;", google_stat_ + ";" + other_stat_);
}

// Garbage collection prioritize removal of expired cookies.
TEST_F(EvictedDomainCookieCounterTest, TestGarbageCollectionWithExpiry) {
  InitCounter(5, 1);
  InitStockCookies();
  // Add all cookies at (relative time) t = 0.
  for (int i = 0; i < 6; ++i)
    Add(cookies_[i]);
  // Evict cookies at t = [1,3,6,10,15].
  for (int i = 0; i < 5; ++i) {
    StepTime(i + 1);
    Evict(cookies_[i]);
  }
  EXPECT_EQ(5u, cookie_counter_->GetStorageSize());  // Reached capacity.
  GotoTime(1200);  // t = 1200, so cookies_[2,3,4] expire.
  // Evict cookies_[5] (not expired) at t = 1200.
  Evict(cookies_[5]);
  // Garbage collection would have taken place, removing 3 expired cookies,
  // so that there's no need to remove more.
  EXPECT_EQ(3u, cookie_counter_->GetStorageSize());
  // Reinstate cookies at t = [1500,1500,(1500),(1500),(1500),1500].
  GotoTime(1500);
  InitStockCookies();  // Refresh cookies, so new cookies expire in the future.
  for (int i = 0; i < 6; ++i)
    Add(cookies_[i]);
  EXPECT_EQ(0u, cookie_counter_->GetStorageSize());
  // Reinstatement delays: [1499,1497,(1494),(1490),(1485),300].
  EXPECT_EQ("1499,1497;300", google_stat_ + ";" + other_stat_);
}

}  // namespace

}  // namespace chrome_browser_net
