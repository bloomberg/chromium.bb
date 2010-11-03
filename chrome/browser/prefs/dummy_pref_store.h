// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_PREFS_DUMMY_PREF_STORE_H_
#define CHROME_BROWSER_PREFS_DUMMY_PREF_STORE_H_
#pragma once

#include "base/basictypes.h"
#include "base/scoped_ptr.h"
#include "chrome/common/pref_store.h"

class DictionaryValue;

// |DummyPrefStore| is a stub implementation of the |PrefStore| interface.
// It allows to get and set the state of the |PrefStore|.
class DummyPrefStore : public PrefStore {
 public:
  DummyPrefStore();
  virtual ~DummyPrefStore() {}

  virtual DictionaryValue* prefs() const { return prefs_.get(); }

  virtual PrefStore::PrefReadError ReadPrefs();

  virtual bool ReadOnly() { return read_only_; }

  virtual bool WritePrefs();

  // Getter and Setter methods for setting and getting the state of the
  // |DummyPrefStore|.
  virtual void set_read_only(bool read_only) { read_only_ = read_only; }
  virtual void set_prefs(DictionaryValue* prefs) { prefs_.reset(prefs); }
  virtual void set_prefs_written(bool status) { prefs_written_ = status; }
  virtual bool get_prefs_written() { return prefs_written_; }

 private:
  scoped_ptr<DictionaryValue> prefs_;

  // Flag that indicates if the PrefStore is read-only
  bool read_only_;

  // Flag that indicates if the method WritePrefs was called.
  bool prefs_written_;

  DISALLOW_COPY_AND_ASSIGN(DummyPrefStore);
};

#endif  // CHROME_BROWSER_PREFS_DUMMY_PREF_STORE_H_
