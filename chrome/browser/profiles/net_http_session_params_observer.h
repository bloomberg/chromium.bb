// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PROFILES_NET_HTTP_SESSION_PARAMS_OBSERVER_H_
#define CHROME_BROWSER_PROFILES_NET_HTTP_SESSION_PARAMS_OBSERVER_H_

#include "base/callback_forward.h"
#include "base/macros.h"
#include "components/prefs/pref_member.h"

class PrefService;

namespace user_prefs {
class PrefRegistrySyncable;
}

// Monitors network-related preferences for changes and applies them to
// globally owned HttpNetworkSessions.
// Profile-owned HttpNetworkSessions are taken care of by a
// UpdateNetParamsCallback passed to the constructor.
// The supplied PrefService must outlive this NetHttpSessionParamsObserver.
// Must be used only on the UI thread.
class NetHttpSessionParamsObserver {
 public:
  // Type of callback which will be called when QUIC is disabled. This
  // callback will be called on the IO thread.
  typedef base::Callback<void()> DisableQuicCallback;

  // |prefs| must be non-NULL and |*prefs| must outlive this.
  // |update_net_params_callback| will be invoked on the IO thread when an
  // observed network parmeter is changed.
  NetHttpSessionParamsObserver(PrefService* prefs,
                               DisableQuicCallback disable_quic_callback);
  ~NetHttpSessionParamsObserver();

  // Register user preferences which NetHttpSessionParamsObserver uses into
  // |registry|.
  static void RegisterProfilePrefs(user_prefs::PrefRegistrySyncable* registry);

 private:
  // Called on UI thread when an observed net pref changes
  void ApplySettings();

  BooleanPrefMember quic_allowed_;
  DisableQuicCallback disable_quic_callback_;

  DISALLOW_COPY_AND_ASSIGN(NetHttpSessionParamsObserver);
};

#endif  // CHROME_BROWSER_PROFILES_NET_HTTP_SESSION_PARAMS_OBSERVER_H_
