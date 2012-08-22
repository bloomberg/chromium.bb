// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_ASYNC_DNS_FIELD_TRIAL_H_
#define CHROME_BROWSER_NET_ASYNC_DNS_FIELD_TRIAL_H_

#include <string>

class CommandLine;
class Profile;

namespace chrome_browser_net {

// Configures the AsyncDns field trial as follows.
// group A (async disabled): use CreateSystemHostResolver
// group B (async enabled):  use CreateAsyncHostResolver
// Returns true if should use CreateAsyncHostResolver.
bool ConfigureAsyncDnsFieldTrial();

}  // namespace chrome_browser_net

#endif  // CHROME_BROWSER_NET_ASYNC_DNS_FIELD_TRIAL_H_
