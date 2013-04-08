// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_VARIATIONS_EULA_ACCEPTED_NOTIFIER_CHROMEOS_H_
#define CHROME_BROWSER_METRICS_VARIATIONS_EULA_ACCEPTED_NOTIFIER_CHROMEOS_H_

#include "chrome/browser/metrics/variations/eula_accepted_notifier.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

// ChromeOS implementation of the EulaAcceptedNotifier.
class EulaAcceptedNotifierChromeos : public EulaAcceptedNotifier,
                                     public content::NotificationObserver {
 public:
  EulaAcceptedNotifierChromeos();
  virtual ~EulaAcceptedNotifierChromeos();

  // EulaAcceptedNotifier overrides:
  virtual bool IsEulaAccepted() OVERRIDE;

 private:
  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Used to listen for the EULA accepted notification.
  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(EulaAcceptedNotifierChromeos);
};

#endif  // CHROME_BROWSER_METRICS_VARIATIONS_EULA_ACCEPTED_NOTIFIER_CHROMEOS_H_
