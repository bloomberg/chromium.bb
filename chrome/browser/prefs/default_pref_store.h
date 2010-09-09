// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_DEFAULT_PREF_STORE_H_
#define CHROME_BROWSER_PREFS_DEFAULT_PREF_STORE_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/common/pref_store.h"


// This PrefStore keeps track of default preference values set when a
// preference is registered with the PrefService.
class DefaultPrefStore : public PrefStore {
 public:
  DefaultPrefStore();
  virtual ~DefaultPrefStore();

  // PrefStore methods:
  virtual DictionaryValue* prefs();
  virtual PrefStore::PrefReadError ReadPrefs();

 private:
  // The default preference values.
  scoped_ptr<DictionaryValue> prefs_;

  DISALLOW_COPY_AND_ASSIGN(DefaultPrefStore);
};

#endif  // CHROME_BROWSER_PREFS_DEFAULT_PREF_STORE_H_
