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
#include "base/threading/thread_checker.h"
#include "base/timer.h"
#include "chrome/browser/extensions/event_router.h"
#include "chrome/browser/idle.h"
#include "chrome/browser/profiles/profile_keyed_service.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

namespace base {
class StringValue;
}  // namespace base

class Profile;

namespace extensions {

typedef base::Callback<void(IdleState)> QueryStateCallback;

struct IdleMonitor {
  explicit IdleMonitor(IdleState initial_state);

  IdleState last_state;
  int listeners;
  int threshold;
};

class IdleManager : public content::NotificationObserver,
                    public EventRouter::Observer,
                    public ProfileKeyedService {
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

  // ProfileKeyedService implementation.
  virtual void Shutdown() OVERRIDE;

  // content::NotificationDelegate implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

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
  base::WeakPtrFactory<IdleManager> weak_factory_;
  content::NotificationRegistrar registrar_;

  scoped_ptr<IdleTimeProvider> idle_time_provider_;
  scoped_ptr<EventDelegate> event_delegate_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(IdleManager);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_IDLE_IDLE_MANAGER_H_
