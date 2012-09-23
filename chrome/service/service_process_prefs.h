// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_SERVICE_PROCESS_PREFS_H_
#define CHROME_SERVICE_SERVICE_PROCESS_PREFS_H_

#include <string>

#include "chrome/common/json_pref_store.h"

namespace base {
class DictionaryValue;
class ListValue;
}

// Manages persistent preferences for the service process. This is basically a
// thin wrapper around JsonPrefStore for more comfortable use.
class ServiceProcessPrefs {
 public:
  // |file_message_loop_proxy| is the MessageLoopProxy for a thread on which
  // file I/O can be done.
  ServiceProcessPrefs(const FilePath& pref_filename,
                      base::MessageLoopProxy* file_message_loop_proxy);
  ~ServiceProcessPrefs();

  // Read preferences from the backing file.
  void ReadPrefs();

  // Write the data to the backing file.
  void WritePrefs();

  // Returns a string preference for |key|.
  std::string GetString(const std::string& key,
                        const std::string& default_value) const;

  // Set a string |value| for |key|.
  void SetString(const std::string& key, const std::string& value);

  // Returns a boolean preference for |key|.
  bool GetBoolean(const std::string& key, bool default_value) const;

  // Set a boolean |value| for |key|.
  void SetBoolean(const std::string& key, bool value);

  // Returns a dictionary preference for |key|.
  const base::DictionaryValue* GetDictionary(const std::string& key) const;

  // Returns a list for |key|.
  const base::ListValue* GetList(const std::string& key) const;

  // Removes the pref specified by |key|.
  void RemovePref(const std::string& key);

 private:
  scoped_refptr<JsonPrefStore> prefs_;

  DISALLOW_COPY_AND_ASSIGN(ServiceProcessPrefs);
};

#endif  // CHROME_SERVICE_SERVICE_PROCESS_PREFS_H_
