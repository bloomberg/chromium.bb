// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_ARC_ARC_SERVICE_MANAGER_H_
#define COMPONENTS_ARC_ARC_SERVICE_MANAGER_H_

#include <memory>
#include <string>
#include <type_traits>
#include <unordered_map>
#include <utility>
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

namespace internal {

// If an ArcService is declared with a name, e.g.:
//
// class MyArcService : public ArcService {
//  public:
//   static const char kArcServiceName[];
//   ...
// };
//
// it can then be retrieved from ArcServiceManager in a type-safe way using
// GetService<T>(). This two functions allow AddService() to get the name only
// if it was provided, or use an empty string otherwise.
//
// Although the typename is always specified explicitly by the caller, the
// parameter is required in order for SFINAE to work correctly.  It is not used
// and can be nullptr, though.
//
// In order to avoid collisions, kArcServiceName should be the fully-qualified
// name of the class.
template <typename T>
decltype(T::kArcServiceName, std::string()) GetArcServiceName(T* unused) {
  if (strlen(T::kArcServiceName) == 0)
    LOG(ERROR) << "kArcServiceName[] should be a fully-qualified class name.";
  return T::kArcServiceName;
}

template <typename T>
std::string GetArcServiceName(...) {
  return std::string();
}

}  // namespace internal

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

  // Adds a service to the managed services list. Returns false if another
  // named service with that name had already been added.
  template <typename T>
  bool AddService(std::unique_ptr<T> service) {
    return AddServiceInternal(internal::GetArcServiceName<T>(nullptr),
                              std::move(service));
  }

  // Gets the named service from the managed services list. This uses SFINAE, so
  // you can only call this function if the service specified by T provides a
  // static member variable called kArcServiceName[] (otherwise this will not
  // compile).
  template <typename T>
  T* GetService() {
    return static_cast<T*>(GetNamedServiceInternal(T::kArcServiceName));
  }

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

  // Helper methods for AddService and GetService.
  bool AddServiceInternal(const std::string& name,
                          std::unique_ptr<ArcService> service);
  ArcService* GetNamedServiceInternal(const std::string& name);

  base::ThreadChecker thread_checker_;
  scoped_refptr<base::TaskRunner> blocking_task_runner_;

  // An object for observing the ArcIntentHelper instance in |services_|.
  std::unique_ptr<ArcIntentHelperObserver> intent_helper_observer_;

  std::unique_ptr<ArcBridgeService> arc_bridge_service_;
  std::unordered_multimap<std::string, std::unique_ptr<ArcService>> services_;
  scoped_refptr<ActivityIconLoader> icon_loader_;
  scoped_refptr<LocalActivityResolver> activity_resolver_;

  base::ObserverList<Observer> observer_list_;

  DISALLOW_COPY_AND_ASSIGN(ArcServiceManager);
};

}  // namespace arc

#endif  // COMPONENTS_ARC_ARC_SERVICE_MANAGER_H_
