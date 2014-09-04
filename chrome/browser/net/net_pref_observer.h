// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NET_PREF_OBSERVER_H_
#define CHROME_BROWSER_NET_NET_PREF_OBSERVER_H_

#include "base/macros.h"
#include "base/prefs/pref_member.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

// Monitors network-related preferences for changes and applies them.
// The supplied PrefService must outlive this NetPrefObserver.
// Must be used only on the UI thread.
class NetPrefObserver {
 public:
  // |prefs| must be non-NULL and |*prefs| must outlive this.
  explicit NetPrefObserver(PrefService* prefs);
  virtual ~NetPrefObserver();

  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  void ApplySettings();

  BooleanPrefMember spdy_disabled_;

  DISALLOW_COPY_AND_ASSIGN(NetPrefObserver);
};

#endif  // CHROME_BROWSER_NET_NET_PREF_OBSERVER_H_
