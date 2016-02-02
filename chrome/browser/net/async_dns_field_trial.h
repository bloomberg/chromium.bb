// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_ASYNC_DNS_FIELD_TRIAL_H_
#define CHROME_BROWSER_NET_ASYNC_DNS_FIELD_TRIAL_H_

#include "components/prefs/pref_service.h"

namespace chrome_browser_net {

// Returns true when the async resolver should be used.
bool ConfigureAsyncDnsFieldTrial();

// Logs the derivation of the async DNS pref's actual value, for debugging.
// Must be called on the UI thread, since it accesses prefs directly.
void LogAsyncDnsPrefSource(const PrefService::Preference* pref);

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_ASYNC_DNS_FIELD_TRIAL_H_
