// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains utility functions to wait for network state.

#ifndef CHROME_BROWSER_CHROMEOS_NET_DELAY_NETWORK_CALL_H_
#define CHROME_BROWSER_CHROMEOS_NET_DELAY_NETWORK_CALL_H_

namespace base {

template <typename T>
class Callback;

typedef Callback<void(void)> Closure;

class TimeDelta;

}  // namespace base

namespace chromeos {

// Default delay to be used as an argument to DelayNetworkCall().
extern const unsigned kDefaultNetworkRetryDelayMS;

// Delay callback until the network is connected or while on a captive portal.
void DelayNetworkCall(const base::Closure& callback, base::TimeDelta retry);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_NET_DELAY_NETWORK_CALL_H_
