// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_SET_OBSERVER_H_
#define CHROME_BROWSER_PREFS_PREF_SET_OBSERVER_H_
#pragma once

#include <set>

#include "base/basictypes.h"
#include "chrome/browser/prefs/pref_change_registrar.h"
#include "chrome/browser/prefs/pref_service.h"
#include "content/common/notification_observer.h"

// Observes the state of a set of preferences and allows to query their combined
// managed bits.
class PrefSetObserver : public NotificationObserver {
 public:
  // Initialize with an empty set of preferences.
  PrefSetObserver(PrefService* pref_service,
                  NotificationObserver* observer);
  virtual ~PrefSetObserver();

  // Add a |pref| to the set of preferences to observe.
  void AddPref(const std::string& pref);
  // Remove |pref| from the set of observed peferences.
  void RemovePref(const std::string& pref);

  // Check whether |pref| is in the set of observed preferences.
  bool IsObserved(const std::string& pref);
  // Check whether any of the observed preferences has the managed bit set.
  bool IsManaged();

  // Create a pref set observer for all preferences relevant to proxies.
  static PrefSetObserver* CreateProxyPrefSetObserver(
      PrefService* pref_service,
      NotificationObserver* observer);

  // Create a pref set observer for all preferences relevant to default search.
  static PrefSetObserver* CreateDefaultSearchPrefSetObserver(
      PrefService* pref_service,
      NotificationObserver* observer);

 private:
  // Overridden from NotificationObserver.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  typedef std::set<std::string> PrefSet;
  PrefSet prefs_;

  PrefService* pref_service_;
  PrefChangeRegistrar registrar_;
  NotificationObserver* observer_;

  DISALLOW_COPY_AND_ASSIGN(PrefSetObserver);
};

#endif  // CHROME_BROWSER_PREFS_PREF_SET_OBSERVER_H_
