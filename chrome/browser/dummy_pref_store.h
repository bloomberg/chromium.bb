// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
#ifndef CHROME_BROWSER_DUMMY_PREF_STORE_H_
#define CHROME_BROWSER_DUMMY_PREF_STORE_H_

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/browser/pref_store.h"

class DummyPrefStore : public PrefStore {
 public:
  DummyPrefStore();

  virtual DictionaryValue* prefs() { return prefs_.get(); }
  void SetPrefs(DictionaryValue* prefs) { prefs_.reset(prefs); }

  virtual PrefStore::PrefReadError ReadPrefs();

 private:
  scoped_ptr<DictionaryValue> prefs_;

  DISALLOW_COPY_AND_ASSIGN(DummyPrefStore);
};

#endif  // CHROME_BROWSER_DUMMY_PREF_STORE_H_
