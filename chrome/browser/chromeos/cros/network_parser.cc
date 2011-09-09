// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/cros/network_parser.h"

#include "base/json/json_writer.h" // for debug output only.
#include "base/stringprintf.h"
#include "base/values.h"
// Needed only for debug output (ConnectionTypeToString).
#include "chrome/browser/chromeos/cros/native_network_constants.h"

namespace chromeos {

namespace {
Network* CreateNewNetwork(
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
}  // namespace

NetworkDeviceParser::NetworkDeviceParser(
    const EnumMapper<PropertyIndex>* mapper) : mapper_(mapper) {
  CHECK(mapper);
}

NetworkDeviceParser::~NetworkDeviceParser() {
}

NetworkDevice* NetworkDeviceParser::CreateDeviceFromInfo(
    const std::string& device_path,
    const DictionaryValue& info) {
  scoped_ptr<NetworkDevice> device(new NetworkDevice(device_path));
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
    Value* value;
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
                                       const Value& value,
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
  scoped_ptr<Network> network(CreateNewNetwork(type, service_path));
  if (network.get())
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
    Value* value;
    bool res = info.GetWithoutPathExpansion(key, &value);
    DCHECK(res);
    if (res)
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
                                 const Value& value,
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

bool NetworkParser::ParseValue(PropertyIndex index,
                               const Value& value,
                               Network* network) {
  switch (index) {
    case PROPERTY_INDEX_TYPE: {
      std::string type_string;
      if (value.GetAsString(&type_string)) {
        ConnectionType type = ParseType(type_string);
        LOG_IF(ERROR, type != network->type())
            << "Network with mismatched type: " << network->service_path()
            << " " << type << " != " << network->type();
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_DEVICE: {
      std::string device_path;
      if (!value.GetAsString(&device_path))
        break;
      network->set_device_path(device_path);
      return true;
    }
    case PROPERTY_INDEX_NAME: {
      std::string name;
      if (value.GetAsString(&name)) {
        network->SetName(name);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_GUID: {
      std::string unique_id;
      if (!value.GetAsString(&unique_id))
        break;
      network->set_unique_id(unique_id);
      return true;
    }
    case PROPERTY_INDEX_PROFILE: {
      // Note: currently this is only provided for non remembered networks.
      std::string profile_path;
      if (!value.GetAsString(&profile_path))
        break;
      network->set_profile_path(profile_path);
      return true;
    }
    case PROPERTY_INDEX_STATE: {
      std::string state_string;
      if (value.GetAsString(&state_string)) {
        network->SetState(ParseState(state_string));
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_MODE: {
      std::string mode_string;
      if (value.GetAsString(&mode_string)) {
        network->mode_ = ParseMode(mode_string);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_ERROR: {
      std::string error_string;
      if (value.GetAsString(&error_string)) {
        network->error_ = ParseError(error_string);
        return true;
      }
      break;
    }
    case PROPERTY_INDEX_CONNECTABLE: {
      bool connectable;
      if (!value.GetAsBoolean(&connectable))
        break;
      network->set_connectable(connectable);
      return true;
    }
    case PROPERTY_INDEX_IS_ACTIVE: {
      bool is_active;
      if (!value.GetAsBoolean(&is_active))
        break;
      network->set_is_active(is_active);
      return true;
    }
    case PROPERTY_INDEX_AUTO_CONNECT: {
      bool auto_connect;
      if (!value.GetAsBoolean(&auto_connect))
        break;
      network->set_auto_connect(auto_connect);
      return true;
    }
    case PROPERTY_INDEX_SAVE_CREDENTIALS: {
      bool save_credentials;
      if (!value.GetAsBoolean(&save_credentials))
        break;
      network->set_save_credentials(save_credentials);
      return true;
    }
    default:
      break;
  }
  return false;
}

}  // namespace chromeos
