// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_PREF_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_PREF_LOADER_H_

#include <memory>
#include <string>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/scoped_observer.h"
#include "base/values.h"
#include "chrome/browser/extensions/external_loader.h"
#include "components/browser_sync/profile_sync_service.h"
#include "components/sync/driver/sync_service_observer.h"
#include "components/sync_preferences/pref_service_syncable_observer.h"

class Profile;

namespace sync_preferences {
class PrefServiceSyncable;
}

namespace extensions {

// A specialization of the ExternalLoader that uses a json file to
// look up which external extensions are registered.
// Instances of this class are expected to be created and destroyed on the UI
// thread and they are expecting public method calls from the UI thread.
class ExternalPrefLoader : public ExternalLoader,
                           public sync_preferences::PrefServiceSyncableObserver,
                           public syncer::SyncServiceObserver {
 public:
  enum Options {
    NONE = 0,

    // Ensure that only root can force an external install by checking
    // that all components of the path to external extensions files are
    // owned by root and not writable by any non-root user.
    ENSURE_PATH_CONTROLLED_BY_ADMIN = 1 << 0,

    // Delay external preference load. It delays default apps installation
    // to not overload the system on first time user login.
    DELAY_LOAD_UNTIL_PRIORITY_SYNC = 1 << 1,
  };

  // |base_path_id| is the directory containing the external_extensions.json
  // file or the standalone extension manifest files. Relative file paths to
  // extension files are resolved relative to this path. |profile| is used to
  // wait priority sync if DELAY_LOAD_UNTIL_PRIORITY_SYNC set.
  ExternalPrefLoader(int base_path_id, Options options, Profile* profile);

  const base::FilePath GetBaseCrxFilePath() override;

 protected:
  ~ExternalPrefLoader() override;

  void StartLoading() override;
  bool IsOptionSet(Options option) {
    return (options_ & option) != 0;
  }

  // The resource id of the base path with the information about the json
  // file containing which extensions to load.
  const int base_path_id_;

  const Options options_;

 private:
  friend class base::RefCountedThreadSafe<ExternalLoader>;

  // sync_preferences::PrefServiceSyncableObserver:
  void OnIsSyncingChanged() override;

  // syncer::SyncServiceObserver
  void OnStateChanged(syncer::SyncService* sync) override;

  // If priority sync ready posts LoadOnFileThread and return true.
  bool PostLoadIfPrioritySyncReady();

  // Post LoadOnFileThread and stop observing for sync service states.
  void PostLoadAndRemoveObservers();

  // Actually searches for and loads candidate standalone extension preference
  // files in the path corresponding to |base_path_id|.
  // Must be called on the file thread.
  void LoadOnFileThread();

  // Extracts the information contained in an external_extension.json file
  // regarding which extensions to install. |prefs| will be modified to
  // receive the extracted extension information.
  // Must be called from the File thread.
  void ReadExternalExtensionPrefFile(base::DictionaryValue* prefs);

  // Extracts the information contained in standalone external extension
  // json files (<extension id>.json) regarding what external extensions
  // to install. |prefs| will be modified to receive the extracted extension
  // information.
  // Must be called from the File thread.
  void ReadStandaloneExtensionPrefFiles(base::DictionaryValue* prefs);

  // The path (coresponding to |base_path_id_| containing the json files
  // describing which extensions to load.
  base::FilePath base_path_;

  // Profile that loads these external prefs.
  // Needed for waiting for waiting priority sync.
  Profile* profile_;

  // Used for registering observer for sync_preferences::PrefServiceSyncable.
  ScopedObserver<sync_preferences::PrefServiceSyncable,
                 sync_preferences::PrefServiceSyncableObserver>
      syncable_pref_observer_;

  DISALLOW_COPY_AND_ASSIGN(ExternalPrefLoader);
};

// A simplified version of ExternalPrefLoader that loads the dictionary
// from json data specified in a string.
class ExternalTestingLoader : public ExternalLoader {
 public:
  ExternalTestingLoader(const std::string& json_data,
                        const base::FilePath& fake_base_path);

  const base::FilePath GetBaseCrxFilePath() override;

 protected:
  void StartLoading() override;

 private:
  friend class base::RefCountedThreadSafe<ExternalLoader>;

  ~ExternalTestingLoader() override;

  base::FilePath fake_base_path_;
  std::unique_ptr<base::DictionaryValue> testing_prefs_;

  DISALLOW_COPY_AND_ASSIGN(ExternalTestingLoader);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_PREF_LOADER_H_
