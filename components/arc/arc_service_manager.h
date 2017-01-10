// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_SERVICE_MANAGER_H_
#define COMPONENTS_ARC_ARC_SERVICE_MANAGER_H_

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/observer_list.h"
#include "base/task_runner.h"
#include "base/threading/thread_checker.h"
#include "components/arc/intent_helper/activity_icon_loader.h"
#include "components/arc/intent_helper/local_activity_resolver.h"

namespace arc {

class ArcBridgeService;
class ArcIntentHelperObserver;
class ArcService;

// Manages creation and destruction of services that communicate with the ARC
// instance via the ArcBridgeService.
class ArcServiceManager {
 public:
  class Observer {
   public:
    // Called when intent filters are added or removed.
    virtual void OnIntentFiltersUpdated() = 0;

   protected:
    virtual ~Observer() = default;
  };

  explicit ArcServiceManager(
      scoped_refptr<base::TaskRunner> blocking_task_runner);
  ~ArcServiceManager();

  // |arc_bridge_service| can only be accessed on the thread that this
  // class was created on.
  ArcBridgeService* arc_bridge_service();

  // Adds a service to the managed services list.
  void AddService(std::unique_ptr<ArcService> service);

  // Gets the global instance of the ARC Service Manager. This can only be
  // called on the thread that this class was created on.
  static ArcServiceManager* Get();

  // Returns if the ARC Service Manager instance exists.
  // DO NOT CALL THIS. This function is a dirty workaround for properly shutting
  // down chrome/browser/chromeos/extensions/file_manager/event_router.cc, and
  // will likely be removed in the future.
  static bool IsInitialized();

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  // Called to shut down all ARC services.
  void Shutdown();

  scoped_refptr<base::TaskRunner> blocking_task_runner() const {
    return blocking_task_runner_;
  }

  // Returns the icon loader owned by ArcServiceManager and shared by services.
  scoped_refptr<ActivityIconLoader> icon_loader() { return icon_loader_; }

  // Returns the activity resolver owned by ArcServiceManager.
  scoped_refptr<LocalActivityResolver> activity_resolver() {
    return activity_resolver_;
  }

  // Returns the IntentHelperObserver instance owned by ArcServiceManager.
  ArcIntentHelperObserver* intent_helper_observer() {
    return intent_helper_observer_.get();
  }

 private:
  class IntentHelperObserverImpl;  // implemented in arc_service_manager.cc.

  base::ThreadChecker thread_checker_;
  scoped_refptr<base::TaskRunner> blocking_task_runner_;

  // An object for observing the ArcIntentHelper instance in |services_|.
  std::unique_ptr<ArcIntentHelperObserver> intent_helper_observer_;

  std::unique_ptr<ArcBridgeService> arc_bridge_service_;
  std::vector<std::unique_ptr<ArcService>> services_;
  scoped_refptr<ActivityIconLoader> icon_loader_;
  scoped_refptr<LocalActivityResolver> activity_resolver_;

  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(ArcServiceManager);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_SERVICE_MANAGER_H_
