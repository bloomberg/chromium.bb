// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_VARIATIONS_EULA_ACCEPTED_NOTIFIER_MOBILE_H_
#define CHROME_BROWSER_METRICS_VARIATIONS_EULA_ACCEPTED_NOTIFIER_MOBILE_H_

#include "chrome/browser/metrics/variations/eula_accepted_notifier.h"
#include "base/prefs/pref_change_registrar.h"

class PrefService;

// Mobile (Android and iOS) implementation of the EulaAcceptedNotifier.
class EulaAcceptedNotifierMobile : public EulaAcceptedNotifier {
 public:
  explicit EulaAcceptedNotifierMobile(PrefService* local_state);
  virtual ~EulaAcceptedNotifierMobile();

  // EulaAcceptedNotifier overrides:
  virtual bool IsEulaAccepted() OVERRIDE;

 private:
  // Callback for EULA accepted pref change notification.
  void OnPrefChanged();

  PrefService* local_state_;

  // Used to listen for the EULA accepted pref change notification.
  PrefChangeRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(EulaAcceptedNotifierMobile);
};

#endif  // CHROME_BROWSER_METRICS_VARIATIONS_EULA_ACCEPTED_NOTIFIER_MOBILE_H_
