// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_CHROME_PREF_SERVICE_BUILDER_H_
#define CHROME_BROWSER_PREFS_CHROME_PREF_SERVICE_BUILDER_H_

#include "chrome/browser/prefs/pref_service_builder.h"

namespace policy {
class PolicyService;
}

class PrefStore;

class ChromePrefServiceBuilder : public PrefServiceBuilder {
 public:
  ChromePrefServiceBuilder();
  virtual ~ChromePrefServiceBuilder();

  // Factory method that creates a new instance of a PrefService for
  // Chrome with the applicable PrefStores. The |pref_filename| points
  // to the user preference file. This is the usual way to create a
  // new PrefService.
  // |extension_pref_store| is used as the source for extension-controlled
  // preferences and may be NULL.
  // |policy_service| is used as the source for mandatory or recommended
  // policies.
  // If |async| is true, asynchronous version is used.
  // Notifies using PREF_INITIALIZATION_COMPLETED in the end. Details is set to
  // the created PrefService or NULL if creation has failed. Note, it is
  // guaranteed that in asynchronous version initialization happens after this
  // function returned.
  PrefService* CreateChromePrefs(
      const FilePath& pref_filename,
      base::SequencedTaskRunner* pref_io_task_runner,
      policy::PolicyService* policy_service,
      PrefStore* extension_prefs,
      bool async);

 private:
  void ResetDefaultState();

  DISALLOW_COPY_AND_ASSIGN(ChromePrefServiceBuilder);
};

#endif  // CHROME_BROWSER_PREFS_CHROME_PREF_SERVICE_BUILDER_H_
