// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROMEOS_COMPONENTS_TETHER_GMS_CORE_NOTIFICATIONS_STATE_TRACKER_H_
#define CHROMEOS_COMPONENTS_TETHER_GMS_CORE_NOTIFICATIONS_STATE_TRACKER_H_

#include <string>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"

namespace chromeos {

namespace tether {

// Tracks the names of potential Tether host devices whose "Google Play
// Services" notifications are disabled. These notifications are required to be
// enabled in order to utilize Instant Tethering.
class GmsCoreNotificationsStateTracker {
 public:
  class Observer {
   public:
    virtual void OnGmsCoreNotificationStateChanged() = 0;
  };

  GmsCoreNotificationsStateTracker();
  virtual ~GmsCoreNotificationsStateTracker();

  // Returns a list of names of all potential Tether hosts which replied that
  // their "Google Play Services" notifications were disabled during the most
  // recent host scan.
  virtual std::vector<std::string>
  GetGmsCoreNotificationsDisabledDeviceNames() = 0;

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 protected:
  void NotifyGmsCoreNotificationStateChanged();

 private:
  base::ObserverList<Observer>::Unchecked observer_list_;

  DISALLOW_COPY_AND_ASSIGN(GmsCoreNotificationsStateTracker);
};

}  // namespace tether

}  // namespace chromeos

#endif  // CHROMEOS_COMPONENTS_TETHER_GMS_CORE_NOTIFICATIONS_STATE_TRACKER_H_
