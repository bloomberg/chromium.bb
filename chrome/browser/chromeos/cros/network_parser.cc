// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/network_parser.h"

#include "base/json/json_writer.h"  // for debug output only.
#include "base/stringprintf.h"
#include "base/values.h"
// Needed only for debug output (ConnectionTypeToString).
#include "chrome/browser/chromeos/cros/native_network_constants.h"

namespace chromeos {

NetworkDeviceParser::NetworkDeviceParser(
    const EnumMapper<PropertyIndex>* mapper) : mapper_(mapper) {
  CHECK(mapper);
}

NetworkDeviceParser::~NetworkDeviceParser() {
}

NetworkDevice* NetworkDeviceParser::CreateDeviceFromInfo(
    const std::string& device_path,
    const DictionaryValue& info) {
  scoped_ptr<NetworkDevice> device(CreateNewNetworkDevice(device_path));
  if (!UpdateDeviceFromInfo(info, device.get())) {
    NOTREACHED() << "Unable to create new device";
    return NULL;
  }
  VLOG(2) << "Created device for path " << device_path;
  return device.release();
}

bool NetworkDeviceParser::UpdateDeviceFromInfo(const DictionaryValue& info,
                                               NetworkDevice* device) {
  for (DictionaryValue::key_iterator iter = info.begin_keys();
       iter != info.end_keys(); ++iter) {
    const std::string& key = *iter;
    base::Value* value;
    bool result = info.GetWithoutPathExpansion(key, &value);
    DCHECK(result);
    if (result)
      UpdateStatus(key, *value, device, NULL);
  }
  if (VLOG_IS_ON(2)) {
    std::string json;
    base::JSONWriter::Write(&info, true, &json);
    VLOG(2) << "Updated device for path "
            << device->device_path() << ": " << json;
  }
  return true;
}

bool NetworkDeviceParser::UpdateStatus(const std::string& key,
                                       const base::Value& value,
                                       NetworkDevice* device,
                                       PropertyIndex* index) {
  PropertyIndex found_index = mapper().Get(key);
  if (index)
    *index = found_index;
  if (!ParseValue(found_index, value, device)) {
    VLOG(1) << "NetworkDeviceParser: Unhandled key: " << key;
    return false;
  }
  if (VLOG_IS_ON(2)) {
    std::string value_json;
    base::JSONWriter::Write(&value, true, &value_json);
    VLOG(2) << "Updated value on device: "
            << device->device_path() << "[" << key << "] = " << value_json;
  }
  return true;
}

NetworkDevice* NetworkDeviceParser::CreateNewNetworkDevice(
    const std::string&device_path) {
  return new NetworkDevice(device_path);
}

//----------- Network Parser -----------------

NetworkParser::NetworkParser(const EnumMapper<PropertyIndex>* mapper)
    : mapper_(mapper) {
  CHECK(mapper);
}

NetworkParser::~NetworkParser() {
}

Network* NetworkParser::CreateNetworkFromInfo(
    const std::string& service_path,
    const DictionaryValue& info) {
  ConnectionType type = ParseTypeFromDictionary(info);
  if (type == TYPE_UNKNOWN)  // Return NULL if cannot parse network type.
    return NULL;
  scoped_ptr<Network> network(CreateNewNetwork(type, service_path));
  UpdateNetworkFromInfo(info, network.get());
  VLOG(2) << "Created Network '" << network->name()
          << "' from info. Path:" << service_path
          << " Type:" << ConnectionTypeToString(type);
  return network.release();
}

bool NetworkParser::UpdateNetworkFromInfo(const DictionaryValue& info,
                                          Network* network) {
  network->set_unique_id("");
  for (DictionaryValue::key_iterator iter = info.begin_keys();
       iter != info.end_keys(); ++iter) {
    const std::string& key = *iter;
    base::Value* value;
    bool res = info.GetWithoutPathExpansion(key, &value);
    DCHECK(res);
    if (res)  // Use network's parser to update status
      network->UpdateStatus(key, *value, NULL);
  }
  if (network->unique_id().empty())
    network->CalculateUniqueId();
  VLOG(2) << "Updated network '" << network->name()
          << "' Path:" << network->service_path() << " Type:"
          << ConnectionTypeToString(network->type());
  return true;
}

bool NetworkParser::UpdateStatus(const std::string& key,
                                 const base::Value& value,
                                 Network* network,
                                 PropertyIndex* index) {
  PropertyIndex found_index = mapper().Get(key);
  if (index)
    *index = found_index;
  if (!ParseValue(found_index, value, network)) {
    VLOG(1) << "Unhandled key '" << key << "' in Network: " << network->name()
            << " ID: " << network->unique_id()
            << " Type: " << ConnectionTypeToString(network->type());
    return false;
  }
  if (VLOG_IS_ON(2)) {
    std::string value_json;
    base::JSONWriter::Write(&value, true, &value_json);
    VLOG(2) << "Updated value on network: "
            << network->unique_id() << "[" << key << "] = " << value_json;
  }
  return true;
}

Network* NetworkParser::CreateNewNetwork(
    ConnectionType type, const std::string& service_path) {
  switch (type) {
    case TYPE_ETHERNET: {
      EthernetNetwork* ethernet = new EthernetNetwork(service_path);
      return ethernet;
    }
    case TYPE_WIFI: {
      WifiNetwork* wifi = new WifiNetwork(service_path);
      return wifi;
    }
    case TYPE_CELLULAR: {
      CellularNetwork* cellular = new CellularNetwork(service_path);
      return cellular;
    }
    case TYPE_VPN: {
      VirtualNetwork* vpn = new VirtualNetwork(service_path);
      return vpn;
    }
    default: {
      // If we try and create a service for which we have an unknown
      // type, then that's a bug, and we will crash.
      LOG(FATAL) << "Unknown service type: " << type;
      return NULL;
    }
  }
}

bool NetworkParser::ParseValue(PropertyIndex index,
                               const base::Value& value,
                               Network* network) {
  switch (index) {
    case PROPERTY_INDEX_NAME: {
      std::string name;
      if (!value.GetAsString(&name))
        return false;
      network->SetName(name);
      return true;
    }
    case PROPERTY_INDEX_GUID: {
      std::string unique_id;
      if (!value.GetAsString(&unique_id))
        return false;
      network->set_unique_id(unique_id);
      return true;
    }
    case PROPERTY_INDEX_AUTO_CONNECT: {
      bool auto_connect;
      if (!value.GetAsBoolean(&auto_connect))
        return false;
      network->set_auto_connect(auto_connect);
      return true;
    }
    case PROPERTY_INDEX_PROXY_CONFIG: {
      std::string proxy_config;
      if (!value.GetAsString(&proxy_config))
        return false;
      network->set_proxy_config(proxy_config);
      return true;
    }
    default:
      break;
  }
  return false;
}

}  // namespace chromeos
