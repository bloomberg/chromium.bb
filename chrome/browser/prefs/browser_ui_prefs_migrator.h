// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_BROWSER_UI_PREFS_MIGRATOR_H_
#define CHROME_BROWSER_PREFS_BROWSER_UI_PREFS_MIGRATOR_H_

#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_store.h"
#include "base/prefs/writeable_pref_store.h"

// When prefs are loaded from disk this class migrates some previously
// dynamically registered preferences to their new home in a statically
// registered dictionary. It can't follow the usual migration pattern because
// it is migrating preferences that aren't registered at migration time and has
// to iterate over a raw DictionaryValue looking for keys to migrate.
//
// Objects of this class delete themselves after the migration.
//
// TODO(dgrogan): Delete this for m41. See http://crbug.com/167256.

class BrowserUIPrefsMigrator : public PrefStore::Observer {
 public:
  explicit BrowserUIPrefsMigrator(WriteablePrefStore* pref_store);

  // Overrides from PrefStore::Observer.
  virtual void OnPrefValueChanged(const std::string& key) OVERRIDE {}
  virtual void OnInitializationCompleted(bool succeeded) OVERRIDE;

 private:
  friend struct base::DefaultDeleter<BrowserUIPrefsMigrator>;

  virtual ~BrowserUIPrefsMigrator();

  WriteablePrefStore* pref_store_;

  DISALLOW_COPY_AND_ASSIGN(BrowserUIPrefsMigrator);
};

#endif  // CHROME_BROWSER_PREFS_BROWSER_UI_PREFS_MIGRATOR_H_
