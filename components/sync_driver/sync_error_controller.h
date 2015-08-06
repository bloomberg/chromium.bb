// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DRIVER_SYNC_ERROR_CONTROLLER_H_
#define COMPONENTS_SYNC_DRIVER_SYNC_ERROR_CONTROLLER_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/observer_list.h"
#include "components/sync_driver/sync_service_observer.h"

namespace sync_driver {
class SyncService;
}

// Keep track of sync errors and expose them to observers in the UI.
class SyncErrorController : public sync_driver::SyncServiceObserver {
 public:
  // The observer class for SyncErrorController lets the controller notify
  // observers when an error arises or changes.
  class Observer {
   public:
    virtual ~Observer() {}
    virtual void OnErrorChanged() = 0;
  };

  explicit SyncErrorController(sync_driver::SyncService* service);
  ~SyncErrorController() override;

  // True if there exists an error worth elevating to the user.
  bool HasError();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // sync_driver::SyncServiceObserver:
  void OnStateChanged() override;

 private:
  sync_driver::SyncService* service_;
  base::ObserverList<Observer, true> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(SyncErrorController);
};

#endif  // COMPONENTS_SYNC_DRIVER_SYNC_ERROR_CONTROLLER_H_
