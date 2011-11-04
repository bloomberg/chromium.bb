// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_PREF_PROXY_CONFIG_TRACKER_H_
#define CHROME_BROWSER_NET_PREF_PROXY_CONFIG_TRACKER_H_
#pragma once

#include "chrome/browser/net/pref_proxy_config_tracker_impl.h"

#if defined(OS_CHROMEOS)
namespace chromeos {
class ProxyConfigServiceImpl;
}
#endif  // defined(OS_CHROMEOS)

#if defined(OS_CHROMEOS)
typedef chromeos::ProxyConfigServiceImpl PrefProxyConfigTracker;
#else
typedef PrefProxyConfigTrackerImpl PrefProxyConfigTracker;
#endif  // defined(OS_CHROMEOS)

#endif  // CHROME_BROWSER_NET_PREF_PROXY_CONFIG_TRACKER_H_
