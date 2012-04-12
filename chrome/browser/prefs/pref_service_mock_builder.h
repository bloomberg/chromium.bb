// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_SERVICE_MOCK_BUILDER_H_
#define CHROME_BROWSER_PREFS_PREF_SERVICE_MOCK_BUILDER_H_
#pragma once

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "chrome/common/persistent_pref_store.h"
#include "chrome/common/pref_store.h"

class CommandLine;
class FilePath;
class PrefService;

namespace base {
class MessageLoopProxy;
}

namespace policy {
class PolicyService;
}

// A helper that allows convenient building of custom PrefServices in tests.
class PrefServiceMockBuilder {
 public:
  PrefServiceMockBuilder();
  ~PrefServiceMockBuilder();

  // Functions for setting the various parameters of the PrefService to build.
  // These take ownership of the |store| parameter.
  PrefServiceMockBuilder& WithManagedPrefs(PrefStore* store);
  PrefServiceMockBuilder& WithExtensionPrefs(PrefStore* store);
  PrefServiceMockBuilder& WithCommandLinePrefs(PrefStore* store);
  PrefServiceMockBuilder& WithUserPrefs(PersistentPrefStore* store);
  PrefServiceMockBuilder& WithRecommendedPrefs(PrefStore* store);

#if defined(ENABLE_CONFIGURATION_POLICY)
  // Set up policy pref stores using the given policy service.
  PrefServiceMockBuilder& WithManagedPolicies(
      policy::PolicyService* service);
  PrefServiceMockBuilder& WithRecommendedPolicies(
      policy::PolicyService* service);
#endif

  // Specifies to use an actual command-line backed command-line pref store.
  PrefServiceMockBuilder& WithCommandLine(CommandLine* command_line);

  // Specifies to use an actual file-backed user pref store.
  PrefServiceMockBuilder& WithUserFilePrefs(const FilePath& prefs_file);
  PrefServiceMockBuilder& WithUserFilePrefs(
      const FilePath& prefs_file,
      base::MessageLoopProxy* message_loop_proxy);

  // Creates the PrefService, invalidating the entire builder configuration.
  PrefService* Create();

 private:
  scoped_refptr<PrefStore> managed_prefs_;
  scoped_refptr<PrefStore> extension_prefs_;
  scoped_refptr<PrefStore> command_line_prefs_;
  scoped_refptr<PersistentPrefStore> user_prefs_;
  scoped_refptr<PrefStore> recommended_prefs_;

  DISALLOW_COPY_AND_ASSIGN(PrefServiceMockBuilder);
};

#endif  // CHROME_BROWSER_PREFS_PREF_SERVICE_MOCK_BUILDER_H_
