// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_NETWORK_PARSER_H_
#define CHROME_BROWSER_CHROMEOS_CROS_NETWORK_PARSER_H_
#pragma once

#include <string>
#include <map>

#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/cros/network_library.h"

namespace base {
class DictionaryValue;
class Value;
}

namespace chromeos {

class NetworkDevice;

// This turns an array of string-to-enum-value mappings into a class
// that can cache the mapping and do quick lookups using an actual map
// class.  Usage is something like:
//
// const char kKey1[] = "key1";
// const char kKey2[] = "key2";
//
// enum EnumFoo {
//   UNKNOWN = 0,
//   FOO = 1,
//   BAR = 2,
// };
//
// const EnumMapper<EnumFoo>::Pair index_table[] = {
//   { kKey1, FOO },
//   { kKey2, BAR },
// };
//
// EnumMapper<EnumFoo> mapper(index_table, arraysize(index_table), UNKNOWN);
// EnumFoo value = mapper.Get(kKey1);  // Returns FOO.
// EnumFoo value = mapper.Get('boo');  // Returns UNKNOWN.
template <typename EnumType>
class EnumMapper {
 public:
  struct Pair {
    const char* key;
    const EnumType value;
  };

  EnumMapper(const Pair* list, size_t num_entries, EnumType unknown)
      : unknown_value_(unknown) {
    for (size_t i = 0; i < num_entries; ++i, ++list) {
      enum_map_[list->key] = list->value;
      inverse_enum_map_[list->value] = list->key;
    }
  }

  EnumType Get(const std::string& type) const {
    EnumMapConstIter iter = enum_map_.find(type);
    if (iter != enum_map_.end())
      return iter->second;
    return unknown_value_;
  }

  std::string GetKey(EnumType type) const {
    InverseEnumMapConstIter iter = inverse_enum_map_.find(type);
    if (iter != inverse_enum_map_.end())
      return iter->second;
    return std::string();
  }

 private:
  typedef typename std::map<std::string, EnumType> EnumMap;
  typedef typename std::map<EnumType, std::string> InverseEnumMap;
  typedef typename EnumMap::const_iterator EnumMapConstIter;
  typedef typename InverseEnumMap::const_iterator InverseEnumMapConstIter;
  EnumMap enum_map_;
  InverseEnumMap inverse_enum_map_;
  EnumType unknown_value_;
  DISALLOW_COPY_AND_ASSIGN(EnumMapper);
};

// This takes a Value of a particular form, and maps the keys in the
// dictionary to a NetworkDevice object to initialize it properly.
// Subclasses of this can then customize its methods to parse either
// libcros (flimflam) data or network setup information obtained from
// policy or setup file import depending on the EnumMapper supplied.
class NetworkDeviceParser {
 public:
  virtual ~NetworkDeviceParser();

  virtual NetworkDevice* CreateDeviceFromInfo(
      const std::string& device_path,
      const base::DictionaryValue& info);
  virtual bool UpdateDeviceFromInfo(const base::DictionaryValue& info,
                                    NetworkDevice* device);
  virtual bool UpdateStatus(const std::string& key,
                            const base::Value& value,
                            NetworkDevice* device,
                            PropertyIndex* index);

 protected:
  // The NetworkDeviceParser does not take ownership of the |mapper|.
  explicit NetworkDeviceParser(const EnumMapper<PropertyIndex>* mapper);

  // Creates new NetworkDevice based on device_path.
  // Subclasses should override this method and set the correct parser for this
  // network device if appropriate.
  virtual NetworkDevice* CreateNewNetworkDevice(const std::string& device_path);

  virtual bool ParseValue(PropertyIndex index,
                          const base::Value& value,
                          NetworkDevice* device) = 0;
  virtual ConnectionType ParseType(const std::string& type) = 0;

  const EnumMapper<PropertyIndex>& mapper() const {
    return *mapper_;
  }

 private:
  const EnumMapper<PropertyIndex>* mapper_;
  DISALLOW_COPY_AND_ASSIGN(NetworkDeviceParser);
};

// This takes a Value of a particular form, and uses the keys in the
// dictionary to create Network (WiFiNetwork, EthernetNetwork, etc.)
// objects and initialize them properly.  Subclasses of this can then
// customize its methods to parse other forms of input dictionaries.
class NetworkParser {
 public:
  virtual ~NetworkParser();

  // Called when a new network is encountered.  In addition to setting the
  // members on the Network object, the Network's property_map_ variable
  // will include all the property and corresponding value in |info|.
  // Returns NULL upon failure.
  virtual Network* CreateNetworkFromInfo(const std::string& service_path,
                                         const base::DictionaryValue& info);

  // Called when an existing network is has new information that needs
  // to be updated.  Network's property_map_ variable will be updated.
  // Returns false upon failure.
  virtual bool UpdateNetworkFromInfo(const base::DictionaryValue& info,
                                     Network* network);

  // Called when an individual attribute of an existing network has
  // changed.  |index| is a return value that supplies the appropriate
  // property index for the given key.  |index| is filled in even if
  // the update fails.  Network's property_map_ variable will be updated.
  // Returns false upon failure.
  virtual bool UpdateStatus(const std::string& key,
                            const base::Value& value,
                            Network* network,
                            PropertyIndex* index);

 protected:
  // The NetworkParser does not take ownership of the |mapper|.
  explicit NetworkParser(const EnumMapper<PropertyIndex>* mapper);

  // Creates new Network based on type and service_path.
  // Subclasses should override this method and set the correct parser for this
  // network if appropriate.
  virtual Network* CreateNewNetwork(ConnectionType type,
                                    const std::string& service_path);

  // Parses the value and sets the appropriate field on Network.
  virtual bool ParseValue(PropertyIndex index,
                          const base::Value& value,
                          Network* network);

  virtual ConnectionType ParseType(const std::string& type) = 0;
  virtual ConnectionType ParseTypeFromDictionary(
      const base::DictionaryValue& info) = 0;

  const EnumMapper<PropertyIndex>& mapper() const {
    return *mapper_;
  }

 private:
  const EnumMapper<PropertyIndex>* mapper_;
  DISALLOW_COPY_AND_ASSIGN(NetworkParser);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_CROS_NETWORK_PARSER_H_
