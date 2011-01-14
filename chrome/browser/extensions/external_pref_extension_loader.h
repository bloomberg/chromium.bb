// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTERNAL_PREF_EXTENSION_LOADER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTERNAL_PREF_EXTENSION_LOADER_H_
#pragma once

#include "chrome/browser/extensions/external_extension_loader.h"

#include <string>

#include "base/scoped_ptr.h"
#include "base/values.h"

// A specialization of the ExternalExtensionLoader that uses a json file to
// look up which external extensions are registered.
// Instances of this class are expected to be created and destroyed on the UI
// thread and they are expecting public method calls from the UI thread.
class ExternalPrefExtensionLoader : public ExternalExtensionLoader {
 public:
  ExternalPrefExtensionLoader();

 protected:
  virtual void StartLoading();

 private:
  friend class base::RefCountedThreadSafe<ExternalExtensionLoader>;

  virtual ~ExternalPrefExtensionLoader() {}

  void LoadOnFileThread();

  DISALLOW_COPY_AND_ASSIGN(ExternalPrefExtensionLoader);
};

// A simplified version of ExternalPrefExtensionLoader that loads the dictionary
// from json data specified in a string.
class ExternalTestingExtensionLoader : public ExternalExtensionLoader {
 public:
  explicit ExternalTestingExtensionLoader(const std::string& json_data);

 protected:
  virtual void StartLoading();

 private:
  friend class base::RefCountedThreadSafe<ExternalExtensionLoader>;

  virtual ~ExternalTestingExtensionLoader() {}

  scoped_ptr<DictionaryValue> testing_prefs_;

  DISALLOW_COPY_AND_ASSIGN(ExternalTestingExtensionLoader);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTERNAL_PREF_EXTENSION_LOADER_H_
