// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_PREFS_H_
#define CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_PREFS_H_
#pragma once

#include <string>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_temp_dir.h"
#include "chrome/common/extensions/extension.h"

class DictionaryValue;
class ExtensionPrefs;
class ExtensionPrefValueMap;
class PrefService;

// This is a test class intended to make it easier to work with ExtensionPrefs
// in tests.
class TestExtensionPrefs {
 public:
  TestExtensionPrefs();
  virtual ~TestExtensionPrefs();

  ExtensionPrefs* prefs() { return prefs_.get(); }
  const ExtensionPrefs& const_prefs() const { return *prefs_.get(); }
  PrefService* pref_service() { return pref_service_.get(); }
  const FilePath& temp_dir() const { return temp_dir_.path(); }

  // This will cause the ExtensionPrefs to be deleted and recreated, based on
  // any existing backing file we had previously created.
  void RecreateExtensionPrefs();

  // Creates a new Extension with the given name in our temp dir, adds it to
  // our ExtensionPrefs, and returns it.
  scoped_refptr<Extension> AddExtension(std::string name);

  // Similar to AddExtension, but takes a dictionary with manifest values.
  scoped_refptr<Extension> AddExtensionWithManifest(
      const DictionaryValue& manifest, Extension::Location location);

  // Similar to AddExtension, this adds a new test Extension. This is useful for
  // cases when you don't need the Extension object, but just the id it was
  // assigned.
  std::string AddExtensionAndReturnId(std::string name);

  PrefService* CreateIncognitoPrefService() const;

 protected:
  ScopedTempDir temp_dir_;
  FilePath preferences_file_;
  FilePath extensions_dir_;
  scoped_ptr<PrefService> pref_service_;
  scoped_ptr<ExtensionPrefs> prefs_;
  scoped_ptr<ExtensionPrefValueMap> extension_pref_value_map_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TestExtensionPrefs);
};

#endif  // CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_PREFS_H_
