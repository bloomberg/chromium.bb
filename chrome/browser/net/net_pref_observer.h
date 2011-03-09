// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NET_PREF_OBSERVER_H_
#define CHROME_BROWSER_NET_NET_PREF_OBSERVER_H_
#pragma once

#include "base/basictypes.h"
#include "chrome/browser/prefs/pref_member.h"
#include "content/common/notification_observer.h"

// Monitors network-related preferences for changes and applies them.
// The supplied PrefService must outlive this NetPrefObserver.
// Must be used only on the UI thread.
class NetPrefObserver : public NotificationObserver {
 public:
  explicit NetPrefObserver(PrefService* prefs);
  ~NetPrefObserver();

  // NotificationObserver
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  static void RegisterPrefs(PrefService* prefs);

 private:
  void ApplySettings();

  BooleanPrefMember dns_prefetching_enabled_;
  BooleanPrefMember spdy_disabled_;

  DISALLOW_COPY_AND_ASSIGN(NetPrefObserver);
};

#endif  // CHROME_BROWSER_NET_NET_PREF_OBSERVER_H_

