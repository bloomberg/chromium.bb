// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_ACCESS_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_ACCESS_H_
#pragma once

#include <string>
#include <vector>

namespace chromeos {
namespace system_access {

// Returns the current timezone ID, such as "US/Pacific".
// Returns an empty string if there's an error.
std::string GetTimezoneID();

// Sets the current timezone ID.
void SetTimezoneID(const std::string& id);

// TODO(satorux): We should rework this with a std::map.
struct MachineInfo {
  MachineInfo() : name_value_size(0), name_values(NULL) {}
  struct NVPair {
    char* name;
    char* value;
  };
  int name_value_size;
  NVPair* name_values;
};

// TODO(satorux): We should rework this with a std::map.
//
// Returns the system hardware info.
// The MachineInfo instance that is returned by this function MUST be
// deleted by calling FreeMachineInfo.
// Returns NULL on error.
MachineInfo* GetMachineInfo();

// TODO(satorux): Remove this.
//
// Deletes a MachineInfo type that was allocated in the ChromeOS dll.
// We need to do this to safely pass data over the dll boundary
// between our .so and Chrome.
void FreeMachineInfo(MachineInfo* info);


// TODO(satorux): We should rename it to something like NVParser.
//
// Implementation class. Defined here to be accessable by tests.
class ChromeOSSystem {
 public:
  typedef std::vector<std::pair<std::string, std::string> > NameValuePairs;

  void AddNVPair(const std::string& key, const std::string& value);

  // Execute tool and insert (key, <output>) into nv_pairs_.
  bool GetSingleValueFromTool(const std::string& tool,
                              const std::string& key);
  // Execute tool, parse the output using ParseNVPairs, and insert the results
  // into nv_pairs_.
  bool ParseNVPairsFromTool(const std::string& tool,
                            const std::string& eq, const std::string& delim);
  // Fills in machine_info from nv_pairs_.
  void SetMachineInfo(MachineInfo* machine_info);

  const NameValuePairs& nv_pairs() const { return nv_pairs_; }

 private:
  // This will parse strings with output in the format:
  // <key><EQ><value><DELIM>[<key><EQ><value>][...]
  // e.g. ParseNVPairs("key1=value1 key2=value2", "=", " ")
  bool ParseNVPairs(const std::string& in_string,
                    const std::string& eq,
                    const std::string& delim);
  // Similar to file_util::ReadFileToString, but executes a command instead.
  bool ExecCmdToString(const std::string& command, std::string* contents);

  NameValuePairs nv_pairs_;
};

}  // namespace system_access
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_ACCESS_H_
