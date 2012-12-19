// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_SERVICE_MOCK_BUILDER_H_
#define CHROME_BROWSER_PREFS_PREF_SERVICE_MOCK_BUILDER_H_

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/persistent_pref_store.h"
#include "base/prefs/pref_store.h"
#include "chrome/browser/prefs/pref_service_builder.h"

class CommandLine;
class FilePath;
class PrefService;

namespace base {
class SequencedTaskRunner;
}

namespace policy {
class PolicyService;
}

// A helper that allows convenient building of custom PrefServices in tests.
class PrefServiceMockBuilder : public PrefServiceBuilder {
 public:
  PrefServiceMockBuilder();
  virtual ~PrefServiceMockBuilder();

#if defined(ENABLE_CONFIGURATION_POLICY)
  // Set up policy pref stores using the given policy service.
  PrefServiceMockBuilder& WithManagedPolicies(
      policy::PolicyService* service);
  PrefServiceMockBuilder& WithRecommendedPolicies(
      policy::PolicyService* service);
#endif

  // Specifies to use an actual command-line backed command-line pref store.
  PrefServiceMockBuilder& WithCommandLine(CommandLine* command_line);

  // Creates a PrefService for testing, invalidating the entire
  // builder configuration.
  virtual PrefService* Create() OVERRIDE;

 private:
  void ResetTestingState();

  DISALLOW_COPY_AND_ASSIGN(PrefServiceMockBuilder);
};

#endif  // CHROME_BROWSER_PREFS_PREF_SERVICE_MOCK_BUILDER_H_
