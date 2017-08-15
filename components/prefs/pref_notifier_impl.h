// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PREF_NOTIFIER_IMPL_H_
#define COMPONENTS_PREFS_PREF_NOTIFIER_IMPL_H_

#include <list>
#include <memory>
#include <string>

#include "base/callback.h"
#include "base/compiler_specific.h"
#include "base/containers/hash_tables.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/threading/thread_checker.h"
#include "components/prefs/base_prefs_export.h"
#include "components/prefs/pref_notifier.h"
#include "components/prefs/pref_observer.h"

class PrefService;

// The PrefNotifier implementation used by the PrefService.
class COMPONENTS_PREFS_EXPORT PrefNotifierImpl : public PrefNotifier {
 public:
  PrefNotifierImpl();
  explicit PrefNotifierImpl(PrefService* pref_service);
  ~PrefNotifierImpl() override;

  // If the pref at the given path changes, we call the observer's
  // OnPreferenceChanged method.
  void AddPrefObserver(const std::string& path, PrefObserver* observer);
  void RemovePrefObserver(const std::string& path, PrefObserver* observer);

  // We run the callback once, when initialization completes. The bool
  // parameter will be set to true for successful initialization,
  // false for unsuccessful.
  void AddInitObserver(base::Callback<void(bool)> observer);

  void SetPrefService(PrefService* pref_service);

 protected:
  // PrefNotifier overrides.
  void OnPreferenceChanged(const std::string& pref_name) override;
  void OnInitializationCompleted(bool succeeded) override;

  // A map from pref names to a list of observers. Observers get fired in the
  // order they are added. These should only be accessed externally for unit
  // testing.
  typedef base::ObserverList<PrefObserver> PrefObserverList;
  typedef base::hash_map<std::string, std::unique_ptr<PrefObserverList>>
      PrefObserverMap;

  typedef std::list<base::Callback<void(bool)>> PrefInitObserverList;

  const PrefObserverMap* pref_observers() const { return &pref_observers_; }

 private:
  // For the given pref_name, fire any observer of the pref. Virtual so it can
  // be mocked for unit testing.
  virtual void FireObservers(const std::string& path);

  // Weak reference; the notifier is owned by the PrefService.
  PrefService* pref_service_;

  PrefObserverMap pref_observers_;
  PrefInitObserverList init_observers_;

  base::ThreadChecker thread_checker_;

  DISALLOW_COPY_AND_ASSIGN(PrefNotifierImpl);
};

#endif  // COMPONENTS_PREFS_PREF_NOTIFIER_IMPL_H_
