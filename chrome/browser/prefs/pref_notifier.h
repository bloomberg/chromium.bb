// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_NOTIFIER_H_
#define CHROME_BROWSER_PREFS_PREF_NOTIFIER_H_
#pragma once

#include <string>
#include <vector>

#include "base/hash_tables.h"
#include "base/non_thread_safe.h"
#include "base/observer_list.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class NotificationObserver;
class PrefService;
class PrefValueStore;
class Value;

// Registers observers for particular preferences and sends notifications when
// preference values or sources (i.e., which preference layer controls the
// preference) change.
class PrefNotifier : public NonThreadSafe,
                     public NotificationObserver {
 public:
  // PrefStores must be listed here in order from highest to lowest priority.
  //   MANAGED contains all managed (policy) preference values.
  //   EXTENSION contains preference values set by extensions.
  //   COMMAND_LINE contains preference values set by command-line switches.
  //   USER contains all user-set preference values.
  //   RECOMMENDED contains all recommended (policy) preference values.
  //   DEFAULT contains all application default preference values.
  // This enum is kept in pref_notifier.h rather than pref_value_store.h in
  // order to minimize additional headers needed by the *PrefStore files.
  enum PrefStoreType {
    // INVALID_STORE is not associated with an actual PrefStore but used as
    // an invalid marker, e.g. as a return value.
    INVALID_STORE = -1,
    MANAGED_STORE = 0,
    EXTENSION_STORE,
    COMMAND_LINE_STORE,
    USER_STORE,
    RECOMMENDED_STORE,
    DEFAULT_STORE,
    PREF_STORE_TYPE_MAX = DEFAULT_STORE
  };

  // The |service| with which this notifier is associated will be sent as the
  // source of any notifications.
  PrefNotifier(PrefService* service, PrefValueStore* value_store);

  virtual ~PrefNotifier();

  // For the given pref_name, fire any observer of the pref if |old_value| is
  // different from the current value, or if the store controlling the value
  // has changed.
  // TODO(pamg): Currently notifies in some cases when it shouldn't. See
  // comments for PrefHasChanged() in pref_value_store.h.
  void OnPreferenceSet(const char* pref_name,
                       PrefNotifier::PrefStoreType new_store,
                       const Value* old_value);

  // Convenience method to be called when a preference is set in the
  // USER_STORE. See OnPreferenceSet().
  void OnUserPreferenceSet(const char* pref_name, const Value* old_value);

  // For the given pref_name, fire any observer of the pref. Virtual so it can
  // be mocked for unit testing.
  virtual void FireObservers(const char* path);

  // If the pref at the given path changes, we call the observer's Observe
  // method with NOTIFY_PREF_CHANGED.
  void AddPrefObserver(const char* path, NotificationObserver* obs);
  void RemovePrefObserver(const char* path, NotificationObserver* obs);

 protected:
  // A map from pref names to a list of observers.  Observers get fired in the
  // order they are added. These should only be accessed externally for unit
  // testing.
  typedef ObserverList<NotificationObserver> NotificationObserverList;
  typedef base::hash_map<std::string, NotificationObserverList*>
      PrefObserverMap;
  const PrefObserverMap* pref_observers() { return &pref_observers_; }

 private:
  // Weak references.
  PrefService* pref_service_;
  PrefValueStore* pref_value_store_;

  NotificationRegistrar registrar_;

  PrefObserverMap pref_observers_;

  // Called after a policy refresh to notify relevant preference observers.
  // |changed_prefs_paths| is the vector of preference paths changed by the
  // policy update. It is passed by value and not reference because
  // this method is called asynchronously as a callback from another thread.
  // Copying the vector guarantees that the vector's lifecycle spans the
  // method's invocation.
  void FireObserversForRefreshedManagedPrefs(
      std::vector<std::string> changed_prefs_paths);

  // NotificationObserver methods:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  DISALLOW_COPY_AND_ASSIGN(PrefNotifier);
};

#endif  // CHROME_BROWSER_PREFS_PREF_NOTIFIER_H_
