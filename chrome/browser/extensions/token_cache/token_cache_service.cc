// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/token_cache/token_cache_service.h"

#include "base/logging.h"
#include "chrome/browser/signin/signin_manager_factory.h"
#include "components/signin/core/browser/signin_manager.h"

using base::Time;
using base::TimeDelta;

namespace extensions {

TokenCacheService::TokenCacheService(Profile* profile) : profile_(profile) {
  SigninManagerFactory::GetForProfile(profile)->AddObserver(this);
}

TokenCacheService::~TokenCacheService() {
}

void TokenCacheService::StoreToken(const std::string& token_name,
                                   const std::string& token_value,
                                   base::TimeDelta time_to_live) {
  TokenCacheData token_data;

  // Get the current time, and make sure that the token has not already expired.
  Time expiration_time;
  TimeDelta zero_delta;

  // Negative time deltas are meaningless to this function.
  DCHECK(time_to_live >= zero_delta);

  if (zero_delta < time_to_live) {
    expiration_time = Time::Now();
    expiration_time += time_to_live;
  }

  token_data.token = token_value;
  token_data.expiration_time = expiration_time;

  // Add the token to our cache, overwriting any existing token with this name.
  token_cache_[token_name] = token_data;
}

// Retrieve a token for the currently logged in user.  This returns an empty
// string if the token was not found or timed out.
std::string TokenCacheService::RetrieveToken(const std::string& token_name) {
  std::map<std::string, TokenCacheData>::iterator it =
      token_cache_.find(token_name);

  if (it != token_cache_.end()) {
    Time now = Time::Now();
    if (it->second.expiration_time.is_null() ||
        now < it->second.expiration_time) {
      return it->second.token;
    } else {
      // Remove this entry if it is expired.
      token_cache_.erase(it);
      return std::string();
    }
  }

  return std::string();
}

void TokenCacheService::Shutdown() {
  SigninManagerFactory::GetForProfile(const_cast<Profile*>(profile_))
      ->RemoveObserver(this);
}

void TokenCacheService::GoogleSignedOut(const std::string& account_id,
                                        const std::string& username) {
  token_cache_.clear();
}

}  // namespace extensions
