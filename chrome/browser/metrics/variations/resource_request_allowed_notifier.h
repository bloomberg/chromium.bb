// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_VARIATIONS_RESOURCE_REQUEST_ALLOWED_NOTIFIER_H_
#define CHROME_BROWSER_METRICS_VARIATIONS_RESOURCE_REQUEST_ALLOWED_NOTIFIER_H_

#include "net/base/network_change_notifier.h"

#if defined(OS_CHROMEOS)
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#endif

// This class informs an interested observer when resource requests over the
// network are permitted.
//
// Currently, the criteria for allowing resource requests are:
//  1. The network is currently available,
//  2. The EULA was accepted by the user (ChromeOS only), and
//  3. The --disable-background-networking command line switch is not set.
//
// Interested services should add themselves as an observer of
// ResourceRequestAllowedNotifier and check ResourceRequestsAllowed() to see if
// requests are permitted. If it returns true, they can go ahead and make their
// request. If it returns false, ResourceRequestAllowedNotifier will notify the
// service when the criteria is met.
//
// If ResourceRequestsAllowed returns true the first time,
// ResourceRequestAllowedNotifier will not notify the service in the future.
//
// Note that this class handles the criteria state for a single service, so
// services should keep their own instance of this class rather than sharing a
// global instance.
class ResourceRequestAllowedNotifier :
#if defined(OS_CHROMEOS)
    public content::NotificationObserver,
#endif
    public net::NetworkChangeNotifier::ConnectionTypeObserver {
 public:
  // Observes resource request allowed state changes.
  class Observer {
   public:
    virtual void OnResourceRequestsAllowed() = 0;
  };

  ResourceRequestAllowedNotifier();
  virtual ~ResourceRequestAllowedNotifier();

  // Sets |observer| as the service to be notified by this instance, and
  // performs initial checks on the criteria. |observer| may not be NULL.
  // This is to be called immediately after construction of an instance of
  // ResourceRequestAllowedNotifier to pass it the interested service.
  void Init(Observer* observer);

  // Returns true iff all resource request criteria are met. If not, this call
  // will set some flags so it knows to notify the observer if the criteria
  // changes. Note that the observer will never be notified unless it calls this
  // method first. This is virtual so it can be overriden for tests.
  virtual bool ResourceRequestsAllowed();

  void SetWasWaitingForNetworkForTesting(bool waiting);
#if defined(OS_CHROMEOS)
  void SetWasWaitingForEulaForTesting(bool waiting);
#endif

 protected:
  // Notifies the observer if all criteria needed for resource requests are met.
  // This is protected so it can be called from subclasses for testing.
  void MaybeNotifyObserver();

#if defined(OS_CHROMEOS)
  // On official builds, returns true iff the EULA needs to be accepted. This
  // always returns false on unofficial builds since there is no notion of a
  // EULA.
  //
  // This is virtual so it can be overriden by test classes to avoid making them
  // aware of the ChromeOS details. This is protected so it call be overriden in
  // subclasses for testing.
  virtual bool NeedsEulaAcceptance();
#endif

 private:
  // net::NetworkChangeNotifier::ConnectionTypeObserver overrides:
  virtual void OnConnectionTypeChanged(
      net::NetworkChangeNotifier::ConnectionType type) OVERRIDE;

#if defined(OS_CHROMEOS)
  // content::NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;
#endif

  // Tracks whether or not the observer/service depending on this class actually
  // requested permission to make a request or not. If it did not, then this
  // class should not notify it even if the criteria is met.
  bool observer_requested_permission_;

  // Tracks network connectivity criteria.
  bool was_waiting_for_network_;

#if defined(OS_CHROMEOS)
  // Tracks EULA acceptance criteria.
  bool was_waiting_for_user_to_accept_eula_;

  // Used to listen for the EULA accepted notification.
  content::NotificationRegistrar registrar_;
#endif

  // Observing service interested in request permissions.
  Observer* observer_;

  DISALLOW_COPY_AND_ASSIGN(ResourceRequestAllowedNotifier);
};

#endif  // CHROME_BROWSER_METRICS_VARIATIONS_RESOURCE_REQUEST_ALLOWED_NOTIFIER_H_
