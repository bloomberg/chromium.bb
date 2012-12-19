// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_PREF_SERVICE_BUILDER_H_
#define CHROME_BROWSER_PREFS_PREF_SERVICE_BUILDER_H_

#include "base/basictypes.h"
#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/persistent_pref_store.h"
#include "base/prefs/pref_store.h"

class FilePath;
class PrefModelAssociator;
class PrefService;

namespace base {
class SequencedTaskRunner;
}

// A class that allows convenient building of custom PrefServices.
class PrefServiceBuilder {
 public:
  PrefServiceBuilder();
  virtual ~PrefServiceBuilder();

  // Functions for setting the various parameters of the PrefService to build.
  // These take ownership of the |store| parameter.
  PrefServiceBuilder& WithManagedPrefs(PrefStore* store);
  PrefServiceBuilder& WithExtensionPrefs(PrefStore* store);
  PrefServiceBuilder& WithCommandLinePrefs(PrefStore* store);
  PrefServiceBuilder& WithUserPrefs(PersistentPrefStore* store);
  PrefServiceBuilder& WithRecommendedPrefs(PrefStore* store);

  // Takes ownership of the associator.
  PrefServiceBuilder& WithSyncAssociator(PrefModelAssociator* associator);

  // Sets up error callback for the PrefService.  A do-nothing default
  // is provided if this is not called.
  PrefServiceBuilder& WithReadErrorCallback(
      const base::Callback<void(PersistentPrefStore::PrefReadError)>&
          read_error_callback);

  // Specifies to use an actual file-backed user pref store.
  PrefServiceBuilder& WithUserFilePrefs(
      const FilePath& prefs_file,
      base::SequencedTaskRunner* task_runner);

  PrefServiceBuilder& WithAsync(bool async);

  // Creates the PrefService, invalidating the entire builder configuration.
  virtual PrefService* Create();

 protected:
  scoped_refptr<PrefStore> managed_prefs_;
  scoped_refptr<PrefStore> extension_prefs_;
  scoped_refptr<PrefStore> command_line_prefs_;
  scoped_refptr<PersistentPrefStore> user_prefs_;
  scoped_refptr<PrefStore> recommended_prefs_;

  // TODO(joi): Would be nice to switch the various places that deal
  // with this pointer to using scoped_ptr::Pass semantics.
  PrefModelAssociator* sync_associator_;

  base::Callback<void(PersistentPrefStore::PrefReadError)> read_error_callback_;

  // Defaults to false.
  bool async_;

 private:
  void ResetDefaultState();

  DISALLOW_COPY_AND_ASSIGN(PrefServiceBuilder);
};

#endif  // CHROME_BROWSER_PREFS_PREF_SERVICE_BUILDER_H_
