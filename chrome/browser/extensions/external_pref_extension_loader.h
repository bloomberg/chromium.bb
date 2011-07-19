// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_PREF_EXTENSION_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_PREF_EXTENSION_LOADER_H_
#pragma once

#include "chrome/browser/extensions/external_extension_loader.h"

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/values.h"

// A specialization of the ExternalExtensionLoader that uses a json file to
// look up which external extensions are registered.
// Instances of this class are expected to be created and destroyed on the UI
// thread and they are expecting public method calls from the UI thread.
class ExternalPrefExtensionLoader : public ExternalExtensionLoader {
 public:
  // |base_path_key| is the directory containing the external_extensions.json
  // file.  Relative file paths to extension files are resolved relative
  // to this path.
  explicit ExternalPrefExtensionLoader(int base_path_key);

  virtual const FilePath GetBaseCrxFilePath();

 protected:
  virtual void StartLoading();

 private:
  friend class base::RefCountedThreadSafe<ExternalExtensionLoader>;

  virtual ~ExternalPrefExtensionLoader() {}

  void LoadOnFileThread();

  int base_path_key_;
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

  virtual const FilePath GetBaseCrxFilePath();

 protected:
  virtual void StartLoading();

 private:
  friend class base::RefCountedThreadSafe<ExternalExtensionLoader>;

  virtual ~ExternalTestingExtensionLoader();

  FilePath fake_base_path_;
  scoped_ptr<DictionaryValue> testing_prefs_;

  DISALLOW_COPY_AND_ASSIGN(ExternalTestingExtensionLoader);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_PREF_EXTENSION_LOADER_H_
