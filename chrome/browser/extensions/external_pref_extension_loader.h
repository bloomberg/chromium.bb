// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_PREF_EXTENSION_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_PREF_EXTENSION_LOADER_H_
#pragma once

#include "chrome/browser/extensions/external_extension_loader.h"

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/values.h"

// A specialization of the ExternalExtensionLoader that uses a json file to
// look up which external extensions are registered.
// Instances of this class are expected to be created and destroyed on the UI
// thread and they are expecting public method calls from the UI thread.
class ExternalPrefExtensionLoader : public ExternalExtensionLoader {
 public:
  enum Options {
    NONE = 0,

    // Ensure that only root can force an external install by checking
    // that all components of the path to external extensions files are
    // owned by root and not writable by any non-root user.
    ENSURE_PATH_CONTROLLED_BY_ADMIN = 1 << 0
  };

  // |base_path_key| is the directory containing the external_extensions.json
  // file.  Relative file paths to extension files are resolved relative
  // to this path.
  explicit ExternalPrefExtensionLoader(int base_path_key, Options options);

  virtual const FilePath GetBaseCrxFilePath() OVERRIDE;

 protected:
  virtual void StartLoading() OVERRIDE;
  bool IsOptionSet(Options option) {
    return (options_ & option) != 0;
  }

 private:
  friend class base::RefCountedThreadSafe<ExternalExtensionLoader>;

  virtual ~ExternalPrefExtensionLoader() {}

  DictionaryValue* ReadJsonPrefsFile();
  void LoadOnFileThread();

  int base_path_key_;
  Options options_;
  FilePath base_path_;

  DISALLOW_COPY_AND_ASSIGN(ExternalPrefExtensionLoader);
};

// A simplified version of ExternalPrefExtensionLoader that loads the dictionary
// from json data specified in a string.
class ExternalTestingExtensionLoader : public ExternalExtensionLoader {
 public:
  ExternalTestingExtensionLoader(
      const std::string& json_data,
      const FilePath& fake_base_path);

  virtual const FilePath GetBaseCrxFilePath() OVERRIDE;

 protected:
  virtual void StartLoading() OVERRIDE;

 private:
  friend class base::RefCountedThreadSafe<ExternalExtensionLoader>;

  virtual ~ExternalTestingExtensionLoader();

  FilePath fake_base_path_;
  scoped_ptr<DictionaryValue> testing_prefs_;

  DISALLOW_COPY_AND_ASSIGN(ExternalTestingExtensionLoader);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_PREF_EXTENSION_LOADER_H_
