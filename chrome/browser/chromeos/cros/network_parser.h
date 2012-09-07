// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_CROS_NETWORK_PARSER_H_
#define CHROME_BROWSER_CHROMEOS_CROS_NETWORK_PARSER_H_

#include <map>
#include <string>

#include "chrome/browser/chromeos/cros/enum_mapper.h"
#include "chrome/browser/chromeos/cros/network_library.h"

namespace base {
class DictionaryValue;
class Value;
}

namespace chromeos {

class NetworkDevice;

// This takes a Value of a particular form, and maps the keys in the dictionary
// to a NetworkDevice object to initialize it properly. Subclasses of this can
// then customize its methods to parse either Shill data or network setup
// information obtained from policy or setup file import depending on the
// EnumMapper supplied.
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
