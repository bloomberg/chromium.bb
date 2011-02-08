// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_SERVICE_SERVICE_PROCESS_PREFS_H_
#define CHROME_SERVICE_SERVICE_PROCESS_PREFS_H_
#pragma once

#include <string>

#include "chrome/common/json_pref_store.h"

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

  // Get a string preference for |key| and store it in |result|.
  void GetString(const std::string& key, std::string* result);

  // Set a string |value| for |key|.
  void SetString(const std::string& key, const std::string& value);

  // Get a boolean preference for |key| and store it in |result|.
  void GetBoolean(const std::string& key, bool* result);

  // Set a boolean |value| for |key|.
  void SetBoolean(const std::string& key, bool value);

  // Get a dictionary preference for |key| and store it in |result|.
  void GetDictionary(const std::string& key, DictionaryValue** result);

 private:
  scoped_refptr<JsonPrefStore> prefs_;

  DISALLOW_COPY_AND_ASSIGN(ServiceProcessPrefs);
};

#endif  // CHROME_SERVICE_SERVICE_PROCESS_PREFS_H_
