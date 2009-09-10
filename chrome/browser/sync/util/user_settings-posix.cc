// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE entry.

// Implement the storage of service tokens in memory

#include "chrome/browser/sync/util/user_settings.h"

namespace browser_sync {

void UserSettings::ClearAllServiceTokens() {
  service_tokens_.clear();
}

void UserSettings::SetAuthTokenForService(const string& email,
    const string& service_name, const string& long_lived_service_token) {
  service_tokens_[service_name] = long_lived_service_token;
}

bool UserSettings::GetLastUserAndServiceToken(const string& service_name,
                                              string* username,
                                              string* service_token) {
  ServiceTokenMap::const_iterator iter = service_tokens_.find(service_name);

  if (iter != service_tokens_.end()) {
    *username = email_;
    *service_token = iter->second;
    return true;
  }

  return false;
}

}  // namespace browser_sync
