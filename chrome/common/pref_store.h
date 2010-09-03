// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PREF_STORE_H_
#define CHROME_COMMON_PREF_STORE_H_
#pragma once

class DictionaryValue;

// This is an abstract interface for reading and writing from/to a persistent
// preference store, used by |PrefService|. An implementation using a JSON file
// can be found in |JsonPrefStore|, while an implementation without any backing
// store (currently used for testing) can be found in |DummyPrefStore|.
class PrefStore {
 public:
  // Unique integer code for each type of error so we can report them
  // distinctly in a histogram.
  // NOTE: Don't change the order here as it will change the server's meaning
  // of the histogram.
  enum PrefReadError {
    PREF_READ_ERROR_NONE = 0,
    PREF_READ_ERROR_JSON_PARSE,
    PREF_READ_ERROR_JSON_TYPE,
    PREF_READ_ERROR_ACCESS_DENIED,
    PREF_READ_ERROR_FILE_OTHER,
    PREF_READ_ERROR_FILE_LOCKED,
    PREF_READ_ERROR_NO_FILE,
    PREF_READ_ERROR_JSON_REPEAT,
    PREF_READ_ERROR_OTHER,
    PREF_READ_ERROR_FILE_NOT_SPECIFIED
  };

  virtual ~PrefStore() { }

  // Whether the store is in a pseudo-read-only mode where changes are not
  // actually persisted to disk.  This happens in some cases when there are
  // read errors during startup.
  virtual bool ReadOnly() { return true; }

  virtual DictionaryValue* prefs() = 0;

  virtual PrefReadError ReadPrefs() = 0;

  virtual bool WritePrefs() { return true; }

  virtual void ScheduleWritePrefs() { }
};

#endif  // CHROME_COMMON_PREF_STORE_H_
