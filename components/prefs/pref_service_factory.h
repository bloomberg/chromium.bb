// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_PREFS_PREF_SERVICE_FACTORY_H_
#define COMPONENTS_PREFS_PREF_SERVICE_FACTORY_H_

#include "base/callback.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "components/prefs/persistent_pref_store.h"
#include "components/prefs/pref_registry.h"
#include "components/prefs/pref_store.h"
#include "components/prefs/pref_value_store.h"
#include "components/prefs/prefs_export.h"

class PrefService;

namespace base {
class FilePath;
class SequencedTaskRunner;
}

// A class that allows convenient building of PrefService.
class COMPONENTS_PREFS_EXPORT PrefServiceFactory {
 public:
  PrefServiceFactory();
  virtual ~PrefServiceFactory();

  // Functions for setting the various parameters of the PrefService to build.
  void set_managed_prefs(const scoped_refptr<PrefStore>& managed_prefs) {
    managed_prefs_ = managed_prefs;
  }
  void set_supervised_user_prefs(
      const scoped_refptr<PrefStore>& supervised_user_prefs) {
    supervised_user_prefs_ = supervised_user_prefs;
  }
  void set_extension_prefs(const scoped_refptr<PrefStore>& extension_prefs) {
    extension_prefs_ = extension_prefs;
  }
  void set_command_line_prefs(
      const scoped_refptr<PrefStore>& command_line_prefs) {
    command_line_prefs_ = command_line_prefs;
  }
  void set_user_prefs(const scoped_refptr<PersistentPrefStore>& user_prefs) {
    user_prefs_ = user_prefs;
  }
  void set_recommended_prefs(
      const scoped_refptr<PrefStore>& recommended_prefs) {
    recommended_prefs_ = recommended_prefs;
  }

  // Sets up error callback for the PrefService.  A do-nothing default
  // is provided if this is not called.
  void set_read_error_callback(
      const base::Callback<void(PersistentPrefStore::PrefReadError)>&
          read_error_callback) {
    read_error_callback_ = read_error_callback;
  }

  // Specifies to use an actual file-backed user pref store.
  void SetUserPrefsFile(const base::FilePath& prefs_file,
                        base::SequencedTaskRunner* task_runner);

  void set_async(bool async) {
    async_ = async;
  }

  // Creates a PrefService object initialized with the parameters from
  // this factory.
  std::unique_ptr<PrefService> Create(
      PrefRegistry* registry,
      std::unique_ptr<PrefValueStore::Delegate> delegate = nullptr);

 protected:
  scoped_refptr<PrefStore> managed_prefs_;
  scoped_refptr<PrefStore> supervised_user_prefs_;
  scoped_refptr<PrefStore> extension_prefs_;
  scoped_refptr<PrefStore> command_line_prefs_;
  scoped_refptr<PersistentPrefStore> user_prefs_;
  scoped_refptr<PrefStore> recommended_prefs_;

  base::Callback<void(PersistentPrefStore::PrefReadError)> read_error_callback_;

  // Defaults to false.
  bool async_;

 private:
  DISALLOW_COPY_AND_ASSIGN(PrefServiceFactory);
};

#endif  // COMPONENTS_PREFS_PREF_SERVICE_FACTORY_H_
