// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/token_cache/token_cache_service.h"
#include "chrome/test/base/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

using base::Time;
using base::TimeDelta;

namespace extensions {

class TokenCacheTest : public testing::Test {
 public:
  TokenCacheTest() : cache_(&profile_) {}
  virtual ~TokenCacheTest() { cache_.Shutdown(); }

  size_t CacheSize() {
    return cache_.token_cache_.size();
  }

  bool HasMatch(const std::string& token_name) {
    return cache_.RetrieveToken(token_name) != std::string();
  }

  void InsertExpiredToken(const std::string& token_name,
                          const std::string& token_value) {
    EXPECT_TRUE(!HasMatch(token_name));

    // Compute a time value for yesterday
    Time now = Time::Now();
    TimeDelta one_day = one_day.FromDays(1);
    Time yesterday = now - one_day;

    TokenCacheService::TokenCacheData token_data;
    token_data.token = token_value;
    token_data.expiration_time = yesterday;

    cache_.token_cache_[token_name] = token_data;
  }

 protected:
  TestingProfile profile_;
  TokenCacheService cache_;
};

TEST_F(TokenCacheTest, SaveTokenTest) {
  TimeDelta zero;
  cache_.StoreToken("foo", "bar", zero);

  EXPECT_EQ(1U, CacheSize());
  EXPECT_TRUE(HasMatch("foo"));
}

TEST_F(TokenCacheTest, RetrieveTokenTest) {
  TimeDelta zero;
  cache_.StoreToken("Mozart", "Eine Kleine Nacht Musik", zero);
  cache_.StoreToken("Bach", "Brandenburg Concerto #3", zero);
  cache_.StoreToken("Beethoven", "Emperor Piano Concerto #5", zero);
  cache_.StoreToken("Handel", "Water Music", zero);
  cache_.StoreToken("Chopin", "Heroic", zero);

  std::string found_token = cache_.RetrieveToken("Chopin");
  EXPECT_EQ("Heroic", found_token);
}

TEST_F(TokenCacheTest, ReplaceTokenTest) {
  TimeDelta zero;
  cache_.StoreToken("Chopin", "Heroic", zero);

  std::string found_token = cache_.RetrieveToken("Chopin");
  EXPECT_EQ("Heroic", found_token);

  cache_.StoreToken("Chopin", "Military", zero);

  found_token = cache_.RetrieveToken("Chopin");
  EXPECT_EQ("Military", found_token);
  EXPECT_EQ(1U, CacheSize());
}

TEST_F(TokenCacheTest, SignoutTest) {
  TimeDelta zero;
  cache_.StoreToken("foo", "bar", zero);

  EXPECT_EQ(1U, CacheSize());
  EXPECT_TRUE(HasMatch("foo"));

  cache_.GoogleSignedOut("foo", "foo");

  EXPECT_EQ(0U, CacheSize());
  EXPECT_FALSE(HasMatch("foo"));
}

TEST_F(TokenCacheTest, TokenExpireTest) {
  // Use the fact that we are friends to insert an expired token.
  InsertExpiredToken("foo", "bar");

  EXPECT_EQ(1U, CacheSize());

  // If we attempt to find the token, the attempt should fail.
  EXPECT_FALSE(HasMatch("foo"));
  EXPECT_EQ(0U, CacheSize());
}

} // namespace extensions
