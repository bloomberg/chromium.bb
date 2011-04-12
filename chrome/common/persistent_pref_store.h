// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_COMMON_PERSISTENT_PREF_STORE_H_
#define CHROME_COMMON_PERSISTENT_PREF_STORE_H_
#pragma once

#include <string>

#include <chrome/common/pref_store.h>

// This interface is complementary to the PrefStore interface, declaring
// additional functionality that adds support for setting values and persisting
// the data to some backing store.
class PersistentPrefStore : public PrefStore {
 public:
  virtual ~PersistentPrefStore() {}

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

  // Equivalent to PrefStore::GetValue but returns a mutable value.
  virtual ReadResult GetMutableValue(const std::string& key,
                                     Value** result) = 0;

  // Triggers a value changed notification. This function needs to be called
  // if one retrieves a list or dictionary with GetMutableValue and change its
  // value. SetValue takes care of notifications itself. Note that
  // ReportValueChanged will trigger notifications even if nothing has changed.
  virtual void ReportValueChanged(const std::string& key) = 0;

  // Sets a |value| for |key| in the store. Assumes ownership of |value|, which
  // must be non-NULL.
  virtual void SetValue(const std::string& key, Value* value) = 0;

  // Same as SetValue, but doesn't generate notifications. This is used by
  // PrefService::GetMutableUserPref() in order to put empty entries
  // into the user pref store. Using SetValue is not an option since existing
  // tests rely on the number of notifications generated.
  virtual void SetValueSilently(const std::string& key, Value* value) = 0;

  // Removes the value for |key|.
  virtual void RemoveValue(const std::string& key) = 0;

  // Whether the store is in a pseudo-read-only mode where changes are not
  // actually persisted to disk.  This happens in some cases when there are
  // read errors during startup.
  virtual bool ReadOnly() const = 0;

  // Reads the preferences from disk.
  virtual PrefReadError ReadPrefs() = 0;

  // Writes the preferences to disk immediately.
  virtual bool WritePrefs() = 0;

  // Schedules an asynchronous write operation.
  virtual void ScheduleWritePrefs() = 0;

  // Lands any pending writes to disk.
  virtual void CommitPendingWrite() = 0;
};

#endif  // CHROME_COMMON_PERSISTENT_PREF_STORE_H_
