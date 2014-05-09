// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_TRACKED_TRACKED_PREFERENCES_MIGRATION_H_
#define CHROME_BROWSER_PREFS_TRACKED_TRACKED_PREFERENCES_MIGRATION_H_

#include <set>
#include <string>

#include "base/callback_forward.h"

class InterceptablePrefFilter;

// Sets up InterceptablePrefFilter::FilterOnLoadInterceptors on
// |unprotected_pref_filter| and |protected_pref_filter| which prevents each
// filter from running their on load operations until the interceptors decide to
// hand the prefs back to them (after migration is complete). |
// (un)protected_store_cleaner| and
// |register_on_successful_(un)protected_store_write_callback| are used to do
// post-migration cleanup tasks. Those should be bound to weak pointers to avoid
// blocking shutdown. The migration framework is resilient to a failed cleanup
// (it will simply try again in the next Chrome run).
void SetupTrackedPreferencesMigration(
    const std::set<std::string>& unprotected_pref_names,
    const std::set<std::string>& protected_pref_names,
    const base::Callback<void(const std::string& key)>&
        unprotected_store_cleaner,
    const base::Callback<void(const std::string& key)>& protected_store_cleaner,
    const base::Callback<void(const base::Closure&)>&
        register_on_successful_unprotected_store_write_callback,
    const base::Callback<void(const base::Closure&)>&
        register_on_successful_protected_store_write_callback,
    InterceptablePrefFilter* unprotected_pref_filter,
    InterceptablePrefFilter* protected_pref_filter);

#endif  // CHROME_BROWSER_PREFS_TRACKED_TRACKED_PREFERENCES_MIGRATION_H_
