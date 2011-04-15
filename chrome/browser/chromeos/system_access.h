// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_SYSTEM_ACCESS_H_
#define CHROME_BROWSER_CHROMEOS_SYSTEM_ACCESS_H_
#pragma once

#include <map>
#include <string>
#include <vector>

#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/observer_list.h"
#include "unicode/timezone.h"

namespace chromeos {

// Machine information is represented as a key-value map.
typedef std::map<std::string, std::string> MachineInfo;

// This class provides access to Chrome OS system APIs such as the
// timezone setting.
class SystemAccess {
 public:
  class Observer {
   public:
    // Called when the timezone has changed. |timezone| is non-null.
    virtual void TimezoneChanged(const icu::TimeZone& timezone) = 0;
  };

  static SystemAccess* GetInstance();

  // Returns the current timezone as an icu::Timezone object.
  const icu::TimeZone& GetTimezone();

  // Sets the current timezone. |timezone| must be non-null.
  void SetTimezone(const icu::TimeZone& timezone);

  // Retrieve the named machine statistic (e.g. "hardware_class").
  // This does not update the statistcs.
  bool GetMachineStatistic(const std::string& name, std::string* result);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

 private:
  friend struct DefaultSingletonTraits<SystemAccess>;

  SystemAccess();
  // Updates the machine statistcs by examining the system.
  void UpdateMachineStatistics();

  scoped_ptr<icu::TimeZone> timezone_;
  ObserverList<Observer> observers_;
  MachineInfo machine_info_;

  DISALLOW_COPY_AND_ASSIGN(SystemAccess);
};

// The parser is used to get machine info as name-value pairs. Defined
// here to be accessable by tests.
class NameValuePairsParser {
 public:
  typedef std::vector<std::pair<std::string, std::string> > NameValuePairs;

  // The obtained machine info will be written into machine_info.
  explicit NameValuePairsParser(MachineInfo* machine_info);

  void AddNameValuePair(const std::string& key, const std::string& value);

  // Executes tool and inserts (key, <output>) into name_value_pairs_.
  bool GetSingleValueFromTool(int argc, const char* argv[],
                              const std::string& key);
  // Executes tool, parses the output using ParseNameValuePairs,
  // and inserts the results into name_value_pairs_.
  bool ParseNameValuePairsFromTool(int argc, const char* argv[],
                                   const std::string& eq,
                                   const std::string& delim);

 private:
  // This will parse strings with output in the format:
  // <key><EQ><value><DELIM>[<key><EQ><value>][...]
  // e.g. ParseNameValuePairs("key1=value1 key2=value2", "=", " ")
  bool ParseNameValuePairs(const std::string& in_string,
                           const std::string& eq,
                           const std::string& delim);

  MachineInfo* machine_info_;

  DISALLOW_COPY_AND_ASSIGN(NameValuePairsParser);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_SYSTEM_ACCESS_H_
