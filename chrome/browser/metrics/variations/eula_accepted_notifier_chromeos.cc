// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/eula_accepted_notifier_chromeos.h"

#include "base/logging.h"
#include "chrome/browser/chromeos/login/startup_utils.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"

EulaAcceptedNotifierChromeos::EulaAcceptedNotifierChromeos() {
}

EulaAcceptedNotifierChromeos::~EulaAcceptedNotifierChromeos() {
}

bool EulaAcceptedNotifierChromeos::IsEulaAccepted() {
  if (chromeos::StartupUtils::IsEulaAccepted())
    return true;

  // Register for the notification, if this is the first time.
  if (registrar_.IsEmpty()) {
    // Note that this must listen on AllSources due to the difficulty in knowing
    // when the WizardController instance is created, and to avoid over-coupling
    // the Chrome OS code with the VariationsService by directly attaching as an
    // observer. This is OK because WizardController is essentially a singleton.
    registrar_.Add(this, chrome::NOTIFICATION_WIZARD_EULA_ACCEPTED,
                   content::NotificationService::AllSources());
  }
  return false;
}

void EulaAcceptedNotifierChromeos::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_WIZARD_EULA_ACCEPTED, type);
  // This should only ever be received once. Remove it after this call.
  DCHECK(!registrar_.IsEmpty());
  registrar_.Remove(this, chrome::NOTIFICATION_WIZARD_EULA_ACCEPTED,
                    content::NotificationService::AllSources());

  NotifyObserver();
}

// static
EulaAcceptedNotifier* EulaAcceptedNotifier::Create() {
#if defined(GOOGLE_CHROME_BUILD)
  return new EulaAcceptedNotifierChromeos;
#else
  return NULL;
#endif
}
