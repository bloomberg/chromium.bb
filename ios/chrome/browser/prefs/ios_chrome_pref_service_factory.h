// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PREFS_IOS_CHROME_PREF_SERVICE_FACTORY_H_
#define IOS_CHROME_BROWSER_PREFS_IOS_CHROME_PREF_SERVICE_FACTORY_H_

#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"

class PrefRegistry;
class PrefService;
class PrefStore;
class TrackedPreferenceValidationDelegate;

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace ios {
class ChromeBrowserState;
}

namespace policy {
class PolicyService;
}

namespace syncable_prefs {
class PrefServiceSyncable;
}

namespace user_prefs {
class PrefRegistrySyncable;
}

// Factory methods that create and initialize a new instance of a PrefService
// for Chrome on iOS with the applicable PrefStores. The |pref_filename| points
// to the user preference file. This is the usual way to create a new
// PrefService. |policy_service| is used as the source for mandatory or
// recommended policies. |pref_registry| keeps the list of registered prefs and
// their default valuers. If |async| is true, asynchronous version is used.
scoped_ptr<PrefService> CreateLocalState(
    const base::FilePath& pref_filename,
    base::SequencedTaskRunner* pref_io_task_runner,
    policy::PolicyService* policy_service,
    const scoped_refptr<PrefRegistry>& pref_registry,
    bool async);

scoped_ptr<syncable_prefs::PrefServiceSyncable> CreateBrowserStatePrefs(
    const base::FilePath& browser_state_path,
    base::SequencedTaskRunner* pref_io_task_runner,
    TrackedPreferenceValidationDelegate* validation_delegate,
    policy::PolicyService* policy_service,
    const scoped_refptr<user_prefs::PrefRegistrySyncable>& pref_registry,
    bool async);

// Creates an incognito copy of |pref_service| that shares most prefs but uses
// a fresh non-persistent overlay for the user pref store.
scoped_ptr<syncable_prefs::PrefServiceSyncable>
CreateIncognitoBrowserStatePrefs(
    syncable_prefs::PrefServiceSyncable* main_pref_store);

#endif  // IOS_CHROME_BROWSER_PREFS_IOS_CHROME_PREF_SERVICE_FACTORY_H_
