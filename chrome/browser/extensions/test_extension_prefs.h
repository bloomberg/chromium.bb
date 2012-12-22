// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_PREFS_H_
#define CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_PREFS_H_

#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/common/extensions/extension.h"

class ExtensionPrefValueMap;
class PrefServiceSyncable;

namespace base {
class DictionaryValue;
class SequencedTaskRunner;
}

namespace extensions {
class ExtensionPrefs;

// This is a test class intended to make it easier to work with ExtensionPrefs
// in tests.
class TestExtensionPrefs {
 public:
  explicit TestExtensionPrefs(base::SequencedTaskRunner* task_runner);
  virtual ~TestExtensionPrefs();

  ExtensionPrefs* prefs() { return prefs_.get(); }
  const ExtensionPrefs& const_prefs() const {
      return *prefs_.get();
  }
  PrefServiceSyncable* pref_service() { return pref_service_.get(); }
  const FilePath& temp_dir() const { return temp_dir_.path(); }
  const FilePath& extensions_dir() const { return extensions_dir_; }

  // This will cause the ExtensionPrefs to be deleted and recreated, based on
  // any existing backing file we had previously created.
  void RecreateExtensionPrefs();

  // Creates a new Extension with the given name in our temp dir, adds it to
  // our ExtensionPrefs, and returns it.
  scoped_refptr<Extension> AddExtension(std::string name);

  // As above, but the extension is an app.
  scoped_refptr<Extension> AddApp(std::string name);

  // Similar to AddExtension, but takes a dictionary with manifest values.
  scoped_refptr<Extension> AddExtensionWithManifest(
      const base::DictionaryValue& manifest,
      Extension::Location location);

  // Similar to AddExtension, but takes a dictionary with manifest values
  // and extension flags.
  scoped_refptr<Extension> AddExtensionWithManifestAndFlags(
      const base::DictionaryValue& manifest,
      Extension::Location location,
      int extra_flags);

  // Similar to AddExtension, this adds a new test Extension. This is useful for
  // cases when you don't need the Extension object, but just the id it was
  // assigned.
  std::string AddExtensionAndReturnId(std::string name);

  PrefServiceSyncable* CreateIncognitoPrefService() const;

  // Allows disabling the loading of preferences of extensions. Becomes
  // active after calling RecreateExtensionPrefs(). Defaults to false.
  void set_extensions_disabled(bool extensions_disabled);

 protected:
  base::ScopedTempDir temp_dir_;
  FilePath preferences_file_;
  FilePath extensions_dir_;
  scoped_ptr<PrefServiceSyncable> pref_service_;
  scoped_ptr<ExtensionPrefs> prefs_;
  scoped_ptr<ExtensionPrefValueMap> extension_pref_value_map_;
  const scoped_refptr<base::SequencedTaskRunner> task_runner_;

 private:
  bool extensions_disabled_;
  DISALLOW_COPY_AND_ASSIGN(TestExtensionPrefs);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_TEST_EXTENSION_PREFS_H_
