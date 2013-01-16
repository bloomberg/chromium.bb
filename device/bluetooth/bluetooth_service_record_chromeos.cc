// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluetooth_service_record_chromeos.h"

#include <string>
#include <vector>

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "device/bluetooth/bluetooth_utils.h"
#include "third_party/libxml/chromium/libxml_utils.h"

namespace {

static const char* kAttributeNode = "attribute";
static const char* kIdAttribute = "id";
static const char* kProtocolDescriptorListId = "0x0004";
static const char* kRfcommUuid = "0x0003";
static const char* kSdpNameId = "0x0100";
static const char* kSequenceNode = "sequence";
static const char* kTextNode = "text";
static const char* kUint8Node = "uint8";
static const char* kUuidId = "0x0001";
static const char* kUuidNode = "uuid";
static const char* kValueAttribute = "value";

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

}  // namespace

namespace chromeos {

BluetoothServiceRecordChromeOS::BluetoothServiceRecordChromeOS(
    const std::string& address,
    const std::string& xml_data) {
  address_ = address;
  supports_rfcomm_ = false;

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
          ExtractChannels(&reader);
        }
      } else if (id == kUuidId) {
        if (AdvanceToTag(&reader, kSequenceNode)) {
          ExtractUuid(&reader);
        }
      }
    }
    // We don't care about anything else here, so find the closing tag
    AdvanceToTag(&reader, kAttributeNode);
  }
}

void BluetoothServiceRecordChromeOS::ExtractChannels(XmlReader* reader) {
  const int start_depth = reader->Depth();
  do {
    if (reader->NodeName() == kSequenceNode) {
      if (AdvanceToTag(reader, kUuidNode)) {
        std::string type;
        if (reader->NodeAttribute(kValueAttribute, &type) &&
            type == kRfcommUuid) {
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
        }
      }
    }
  } while (AdvanceToTag(reader, kSequenceNode) &&
           reader->Depth() != start_depth);
}

void BluetoothServiceRecordChromeOS::ExtractUuid(XmlReader* reader) {
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
