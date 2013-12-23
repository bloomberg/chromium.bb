// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_PREF_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_PREF_LOADER_H_

#include "chrome/browser/extensions/external_loader.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"

namespace extensions {

// A specialization of the ExternalLoader that uses a json file to
// look up which external extensions are registered.
// Instances of this class are expected to be created and destroyed on the UI
// thread and they are expecting public method calls from the UI thread.
class ExternalPrefLoader : public ExternalLoader {
 public:
  enum Options {
    NONE = 0,

    // Ensure that only root can force an external install by checking
    // that all components of the path to external extensions files are
    // owned by root and not writable by any non-root user.
    ENSURE_PATH_CONTROLLED_BY_ADMIN = 1 << 0
  };

  // |base_path_id| is the directory containing the external_extensions.json
  // file or the standalone extension manifest files. Relative file paths to
  // extension files are resolved relative to this path.
  ExternalPrefLoader(int base_path_id, Options options);

  virtual const base::FilePath GetBaseCrxFilePath() OVERRIDE;

 protected:
  virtual ~ExternalPrefLoader() {}

  virtual void StartLoading() OVERRIDE;
  bool IsOptionSet(Options option) {
    return (options_ & option) != 0;
  }

  // The resource id of the base path with the information about the json
  // file containing which extensions to load.
  const int base_path_id_;

  const Options options_;

 private:
  friend class base::RefCountedThreadSafe<ExternalLoader>;

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

  DISALLOW_COPY_AND_ASSIGN(ExternalPrefLoader);
};

// A simplified version of ExternalPrefLoader that loads the dictionary
// from json data specified in a string.
class ExternalTestingLoader : public ExternalLoader {
 public:
  ExternalTestingLoader(const std::string& json_data,
                        const base::FilePath& fake_base_path);

  virtual const base::FilePath GetBaseCrxFilePath() OVERRIDE;

 protected:
  virtual void StartLoading() OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<ExternalLoader>;

  virtual ~ExternalTestingLoader();

  base::FilePath fake_base_path_;
  scoped_ptr<base::DictionaryValue> testing_prefs_;

  DISALLOW_COPY_AND_ASSIGN(ExternalTestingLoader);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_PREF_LOADER_H_
