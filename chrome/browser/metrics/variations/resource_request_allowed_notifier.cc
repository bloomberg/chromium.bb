// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/variations/resource_request_allowed_notifier.h"

#include "base/command_line.h"
#include "base/metrics/histogram.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/notification_service.h"

#if defined(OS_CHROMEOS)
#include "chrome/browser/chromeos/login/wizard_controller.h"
#endif

ResourceRequestAllowedNotifier::ResourceRequestAllowedNotifier()
    : observer_requested_permission_(false),
      was_waiting_for_network_(false),
#if defined(OS_CHROMEOS)
      was_waiting_for_user_to_accept_eula_(false),
#endif
      observer_(NULL) {
}

ResourceRequestAllowedNotifier::~ResourceRequestAllowedNotifier() {
  if (observer_)
    net::NetworkChangeNotifier::RemoveConnectionTypeObserver(this);
}

void ResourceRequestAllowedNotifier::Init(Observer* observer) {
  DCHECK(!observer_ && observer);
  observer_ = observer;

  net::NetworkChangeNotifier::AddConnectionTypeObserver(this);

  // Check this state during initialization. It is not expected to change until
  // the corresponding notification is received.
  was_waiting_for_network_ = net::NetworkChangeNotifier::IsOffline();
#if defined(OS_CHROMEOS)
  was_waiting_for_user_to_accept_eula_ = NeedsEulaAcceptance();
  if (was_waiting_for_user_to_accept_eula_) {
    // Note that this must listen on AllSources due to the difficulty in knowing
    // when the WizardController instance is created, and to avoid over-coupling
    // the Chrome OS code with the VariationsService by directly attaching as an
    // observer. This is OK because WizardController is essentially a singleton.
    registrar_.Add(this, chrome::NOTIFICATION_WIZARD_EULA_ACCEPTED,
                   content::NotificationService::AllSources());
  }
#endif
}

bool ResourceRequestAllowedNotifier::ResourceRequestsAllowed() {
  if (CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableBackgroundNetworking)) {
    return false;
  }

  // The observer requested permission. Return the current criteria state and
  // set a flag to remind this class to notify the observer once the criteria
  // is met.
  observer_requested_permission_ = true;
  return
#if defined(OS_CHROMEOS)
      !was_waiting_for_user_to_accept_eula_ &&
#endif
      !was_waiting_for_network_;
}

void ResourceRequestAllowedNotifier::
    SetWasWaitingForNetworkForTesting(bool waiting) {
  was_waiting_for_network_ = waiting;
}

#if defined(OS_CHROMEOS)
void ResourceRequestAllowedNotifier::
    SetWasWaitingForEulaForTesting(bool waiting) {
  was_waiting_for_user_to_accept_eula_ = waiting;
}
#endif

void ResourceRequestAllowedNotifier::MaybeNotifyObserver() {
  // Need to ensure that all criteria are met before notifying observers.
  if (observer_requested_permission_ && ResourceRequestsAllowed()) {
    DVLOG(1) << "Notifying observer of state change.";
    observer_->OnResourceRequestsAllowed();
    // Reset this so the observer is not informed again unless they check
    // ResourceRequestsAllowed again.
    observer_requested_permission_ = false;
  }
}

#if defined(OS_CHROMEOS)
bool ResourceRequestAllowedNotifier::NeedsEulaAcceptance() {
#if defined(GOOGLE_CHROME_BUILD)
  return !chromeos::WizardController::IsEulaAccepted();
#else
  // On unofficial builds, there is no notion of a EULA.
  return false;
#endif
}
#endif

void ResourceRequestAllowedNotifier::OnConnectionTypeChanged(
    net::NetworkChangeNotifier::ConnectionType type) {
  // Only attempt to notify observers if this was previously waiting for the
  // network to reconnect, and new network state is actually available. This
  // prevents the notifier from notifying the observer if the notifier was never
  // waiting on the network, or if the network changes from one online state
  // to another (for example, Wifi to 3G, or Wifi to Wifi, if the network were
  // flaky).
  if (was_waiting_for_network_ &&
      type != net::NetworkChangeNotifier::CONNECTION_NONE) {
    was_waiting_for_network_ = false;
    DVLOG(1) << "Network came back online.";
    MaybeNotifyObserver();
  }
}

#if defined(OS_CHROMEOS)
void ResourceRequestAllowedNotifier::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_WIZARD_EULA_ACCEPTED, type);
  // This should only ever be received once. Remove it after this call.
  DCHECK(!registrar_.IsEmpty());
  registrar_.Remove(this, chrome::NOTIFICATION_WIZARD_EULA_ACCEPTED,
                    content::NotificationService::AllSources());

  // This flag should have been set if this was waiting on the EULA
  // notification.
  DCHECK(was_waiting_for_user_to_accept_eula_);
  DVLOG(1) << "EULA was accepted.";
  was_waiting_for_user_to_accept_eula_ = false;
  MaybeNotifyObserver();
}
#endif
