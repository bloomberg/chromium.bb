// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NET_NET_PREF_OBSERVER_H_
#define CHROME_BROWSER_NET_NET_PREF_OBSERVER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "chrome/browser/prefs/pref_member.h"
#include "content/public/browser/notification_observer.h"

namespace chrome_browser_net {
class Predictor;
}

namespace prerender {
class PrerenderManager;
}

// Monitors network-related preferences for changes and applies them.
// The supplied PrefService must outlive this NetPrefObserver.
// Must be used only on the UI thread.
class NetPrefObserver : public content::NotificationObserver {
 public:
  // |prefs| must be non-NULL and |*prefs| must outlive this.
  // |prerender_manager| may be NULL. If not, |*prerender_manager| must
  // outlive this.
  NetPrefObserver(PrefService* prefs,
                  prerender::PrerenderManager* prerender_manager,
                  chrome_browser_net::Predictor* predictor);
  virtual ~NetPrefObserver();

  // content::NotificationObserver
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  static void RegisterPrefs(PrefService* prefs);

 private:
  // If |pref_name| is NULL, all monitored preferences will be applied.
  void ApplySettings(const std::string* pref_name);

  BooleanPrefMember network_prediction_enabled_;
  BooleanPrefMember spdy_disabled_;
  BooleanPrefMember http_throttling_enabled_;
  prerender::PrerenderManager* prerender_manager_;
  chrome_browser_net::Predictor* predictor_;

  DISALLOW_COPY_AND_ASSIGN(NetPrefObserver);
};

#endif  // CHROME_BROWSER_NET_NET_PREF_OBSERVER_H_
