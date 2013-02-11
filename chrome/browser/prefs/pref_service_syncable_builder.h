// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_SERVICE_SYNCABLE_BUILDER_H_
#define CHROME_BROWSER_PREFS_PREF_SERVICE_SYNCABLE_BUILDER_H_

#include "base/prefs/pref_service_builder.h"

class CommandLine;
class PrefRegistrySyncable;
class PrefServiceSyncable;

namespace policy {
class PolicyService;
}

// A PrefServiceBuilder that also knows how to build a
// PrefServiceSyncable, and may know about Chrome concepts such as
// PolicyService.
class PrefServiceSyncableBuilder : public PrefServiceBuilder {
 public:
  PrefServiceSyncableBuilder();
  virtual ~PrefServiceSyncableBuilder();

#if defined(ENABLE_CONFIGURATION_POLICY)
  // Set up policy pref stores using the given policy service.
  PrefServiceSyncableBuilder& WithManagedPolicies(
      policy::PolicyService* service);
  PrefServiceSyncableBuilder& WithRecommendedPolicies(
      policy::PolicyService* service);
#endif

  // Specifies to use an actual command-line backed command-line pref store.
  PrefServiceSyncableBuilder& WithCommandLine(CommandLine* command_line);

  virtual PrefServiceSyncable* CreateSyncable(PrefRegistrySyncable* registry);

 private:
  DISALLOW_COPY_AND_ASSIGN(PrefServiceSyncableBuilder);
};

#endif  // CHROME_BROWSER_PREFS_PREF_SERVICE_SYNCABLE_BUILDER_H_
