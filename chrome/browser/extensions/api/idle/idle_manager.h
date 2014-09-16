// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_IDLE_IDLE_MANAGER_H_
#define CHROME_BROWSER_EXTENSIONS_API_IDLE_IDLE_MANAGER_H_

#include <map>
#include <string>

#include "base/callback_forward.h"
#include "base/gtest_prod_util.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "base/threading/thread_checker.h"
#include "base/timer/timer.h"
#include "chrome/browser/idle.h"
#include "components/keyed_service/core/keyed_service.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_registry_observer.h"

namespace base {
class StringValue;
}  // namespace base

class Profile;

namespace extensions {
class ExtensionRegistry;

typedef base::Callback<void(IdleState)> QueryStateCallback;

struct IdleMonitor {
  explicit IdleMonitor(IdleState initial_state);

  IdleState last_state;
  int listeners;
  int threshold;
};

class IdleManager : public ExtensionRegistryObserver,
                    public EventRouter::Observer,
                    public KeyedService {
 public:
  class IdleTimeProvider {
   public:
    IdleTimeProvider() {}
    virtual ~IdleTimeProvider() {}
    virtual void CalculateIdleState(int idle_threshold,
                                    IdleCallback notify) = 0;
    virtual void CalculateIdleTime(IdleTimeCallback notify) = 0;
    virtual bool CheckIdleStateIsLocked() = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(IdleTimeProvider);
  };

  class EventDelegate {
   public:
    EventDelegate() {}
    virtual ~EventDelegate() {}
    virtual void OnStateChanged(const std::string& extension_id,
                                IdleState new_state) = 0;
    virtual void RegisterObserver(EventRouter::Observer* observer) = 0;
    virtual void UnregisterObserver(EventRouter::Observer* observer) = 0;

   private:
    DISALLOW_COPY_AND_ASSIGN(EventDelegate);
  };

  explicit IdleManager(Profile* profile);
  virtual ~IdleManager();

  void Init();

  // KeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // ExtensionRegistryObserver implementation.
  virtual void OnExtensionUnloaded(
      content::BrowserContext* browser_context,
      const Extension* extension,
      UnloadedExtensionInfo::Reason reason) OVERRIDE;

  // EventRouter::Observer implementation.
  virtual void OnListenerAdded(const EventListenerInfo& details) OVERRIDE;
  virtual void OnListenerRemoved(const EventListenerInfo& details) OVERRIDE;

  void QueryState(int threshold, QueryStateCallback notify);
  void SetThreshold(const std::string& extension_id, int threshold);
  static base::StringValue* CreateIdleValue(IdleState idle_state);

  // Override default event class. Callee assumes ownership. Used for testing.
  void SetEventDelegateForTest(scoped_ptr<EventDelegate> event_delegate);

  // Override default idle time calculations. Callee assumes ownership. Used
  // for testing.
  void SetIdleTimeProviderForTest(scoped_ptr<IdleTimeProvider> idle_provider);

 private:
  FRIEND_TEST_ALL_PREFIXES(IdleTest, ActiveToIdle);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, ActiveToLocked);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, IdleToActive);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, IdleToLocked);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, LockedToActive);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, LockedToIdle);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, MultipleExtensions);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, ReAddListener);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, SetDetectionInterval);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, SetDetectionIntervalBeforeListener);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, SetDetectionIntervalMaximum);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, SetDetectionIntervalMinimum);
  FRIEND_TEST_ALL_PREFIXES(IdleTest, UnloadCleanup);

  typedef std::map<const std::string, IdleMonitor> MonitorMap;

  IdleMonitor* GetMonitor(const std::string& extension_id);
  void StartPolling();
  void StopPolling();
  void UpdateIdleState();
  void UpdateIdleStateCallback(int idle_time);

  Profile* profile_;

  IdleState last_state_;
  MonitorMap monitors_;

  base::RepeatingTimer<IdleManager> poll_timer_;

  scoped_ptr<IdleTimeProvider> idle_time_provider_;
  scoped_ptr<EventDelegate> event_delegate_;

  base::ThreadChecker thread_checker_;

  // Listen to extension unloaded notification.
  ScopedObserver<ExtensionRegistry, ExtensionRegistryObserver>
      extension_registry_observer_;

  base::WeakPtrFactory<IdleManager> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(IdleManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDLE_IDLE_MANAGER_H_
