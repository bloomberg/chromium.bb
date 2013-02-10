// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_CHROME_PREF_SERVICE_FACTORY_H_
#define CHROME_BROWSER_PREFS_CHROME_PREF_SERVICE_FACTORY_H_

#include "base/memory/ref_counted.h"

namespace base {
class FilePath;
class SequencedTaskRunner;
}

namespace policy {
class PolicyService;
}

class PrefRegistry;
class PrefRegistrySyncable;
class PrefService;
class PrefServiceSyncable;
class PrefStore;

namespace chrome_prefs {

// Factory methods that create and initialize a new instance of a
// PrefService for Chrome with the applicable PrefStores. The
// |pref_filename| points to the user preference file. This is the
// usual way to create a new PrefService.
// |extension_pref_store| is used as the source for extension-controlled
// preferences and may be NULL.
// |policy_service| is used as the source for mandatory or recommended
// policies.
// |pref_registry| keeps the list of registered prefs and their default values.
// If |async| is true, asynchronous version is used.
// Notifies using PREF_INITIALIZATION_COMPLETED in the end. Details is set to
// the created PrefService or NULL if creation has failed. Note, it is
// guaranteed that in asynchronous version initialization happens after this
// function returned.

PrefService* CreateLocalState(
    const base::FilePath& pref_filename,
    base::SequencedTaskRunner* pref_io_task_runner,
    policy::PolicyService* policy_service,
    const scoped_refptr<PrefStore>& extension_prefs,
    const scoped_refptr<PrefRegistry>& pref_registry,
    bool async);

PrefServiceSyncable* CreateProfilePrefs(
    const base::FilePath& pref_filename,
    base::SequencedTaskRunner* pref_io_task_runner,
    policy::PolicyService* policy_service,
    const scoped_refptr<PrefStore>& extension_prefs,
    const scoped_refptr<PrefRegistrySyncable>& pref_registry,
    bool async);

}  // namespace chrome_prefs

#endif  // CHROME_BROWSER_PREFS_CHROME_PREF_SERVICE_FACTORY_H_
