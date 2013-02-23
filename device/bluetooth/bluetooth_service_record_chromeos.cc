// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_service_record_chromeos.h"

#include <bluetooth/bluetooth.h>

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "device/bluetooth/bluetooth_utils.h"
#include "third_party/libxml/chromium/libxml_utils.h"

namespace {

static const char* kAttributeNode = "attribute";
static const char* kBooleanNode = "boolean";
static const char* kSequenceNode = "sequence";
static const char* kTextNode = "text";
static const char* kUint8Node = "uint8";
static const char* kUuidNode = "uuid";

static const char* kIdAttribute = "id";
static const char* kValueAttribute = "value";
static const char* kValueTrue = "true";

static const char* kHidNormallyConnectableId = "0x020d";
static const char* kHidReconnectInitiateId = "0x0205";
static const char* kProtocolDescriptorListId = "0x0004";
static const char* kSdpNameId = "0x0100";
static const char* kServiceClassUuidId = "0x0001";

static const char* kProtocolRfcommUuid = "0x0003";
static const char* kProtocolHidpUuid = "0x0011";

bool AdvanceToTag(XmlReader* reader, const char* node_type) {
  do {
    if (!reader->Read())
      return false;
  } while (reader->NodeName() != node_type);
  return true;
}

bool ExtractTextValue(XmlReader* reader, std::string* value_out) {
  if (AdvanceToTag(reader, kTextNode)) {
    reader->NodeAttribute(kValueAttribute, value_out);
    return true;
  }
  return false;
}

bool ExtractBooleanValue(XmlReader* reader, bool* value_out) {
  if (AdvanceToTag(reader, kBooleanNode)) {
    std::string str_value;
    if (!reader->NodeAttribute(kValueAttribute, &str_value))
      return false;
    *value_out = str_value == kValueTrue;
    return true;
  }
  return false;
}

}  // namespace

namespace chromeos {

BluetoothServiceRecordChromeOS::BluetoothServiceRecordChromeOS(
    const std::string& address,
    const std::string& xml_data) {
  address_ = address;
  supports_rfcomm_ = false;
  supports_hid_ = false;

  // For HID services the default is false when the attribute is not present.
  hid_reconnect_initiate_ = false;
  hid_normally_connectable_ = false;

  XmlReader reader;
  if (!reader.Load(xml_data))
    return;

  while (AdvanceToTag(&reader, kAttributeNode)) {
    std::string id;
    if (reader.NodeAttribute(kIdAttribute, &id)) {
      if (id == kSdpNameId) {
        ExtractTextValue(&reader, &name_);
      } else if (id == kProtocolDescriptorListId) {
        if (AdvanceToTag(&reader, kSequenceNode)) {
          ExtractProtocolDescriptors(&reader);
        }
      } else if (id == kServiceClassUuidId) {
        if (AdvanceToTag(&reader, kSequenceNode)) {
          ExtractServiceClassUuid(&reader);
        }
      } else if (id == kHidNormallyConnectableId) {
        ExtractBooleanValue(&reader, &hid_normally_connectable_);
      } else if (id == kHidReconnectInitiateId) {
        ExtractBooleanValue(&reader, &hid_reconnect_initiate_);
      }
    }
    // We don't care about anything else here, so find the closing tag
    AdvanceToTag(&reader, kAttributeNode);
  }
  if (!supports_hid_) {
    // For non-HID services the default is true.
    hid_normally_connectable_ = true;
    hid_reconnect_initiate_ = true;
  }
}

void BluetoothServiceRecordChromeOS::GetBluetoothAddress(
    bdaddr_t* out_address) const {
  std::string numbers_only;
  for (int i = 0; i < 6; ++i)
    numbers_only += address_.substr(i * 3, 2);

  std::vector<uint8> address_bytes;
  base::HexStringToBytes(numbers_only, &address_bytes);
  for (int i = 0; i < 6; ++i)
    out_address->b[5 - i] = address_bytes[i];
}

void BluetoothServiceRecordChromeOS::ExtractProtocolDescriptors(
    XmlReader* reader) {
  const int start_depth = reader->Depth();
  // The ProtocolDescriptorList can have one or more sequence of sequence of
  // stack, where each stack starts with an UUID and the remaining tags (if
  // present) are protocol-specific.
  do {
    if (reader->NodeName() == kSequenceNode) {
      if (AdvanceToTag(reader, kUuidNode)) {
        std::string protocolUuid;
        if (reader->NodeAttribute(kValueAttribute, &protocolUuid)) {
          // Per protocol parameters parsing.
          if (protocolUuid == kProtocolRfcommUuid) {
            if (AdvanceToTag(reader, kUint8Node)) {
              std::string channel_string;
              if (reader->NodeAttribute(kValueAttribute, &channel_string)) {
                std::vector<uint8> channel_bytes;
                if (base::HexStringToBytes(channel_string.substr(2),
                      &channel_bytes)) {
                  if (channel_bytes.size() == 1) {
                    rfcomm_channel_ = channel_bytes[0];
                    supports_rfcomm_ = true;
                  }
                }
              }
            }
          } else if (protocolUuid == kProtocolHidpUuid) {
            supports_hid_ = true;
          }
        }
      }
    }
  } while (AdvanceToTag(reader, kSequenceNode) &&
           reader->Depth() != start_depth);
}

void BluetoothServiceRecordChromeOS::ExtractServiceClassUuid(
    XmlReader* reader) {
  const int start_depth = reader->Depth();
  do {
    if (reader->NodeName() == kSequenceNode) {
      if (AdvanceToTag(reader, kUuidNode)) {
        if (!reader->NodeAttribute(kValueAttribute, &uuid_))
          uuid_.clear();
      }
    }
  } while (AdvanceToTag(reader, kSequenceNode) &&
           reader->Depth() != start_depth);

  uuid_ = device::bluetooth_utils::CanonicalUuid(uuid_);
}

}  // namespace chromeos
