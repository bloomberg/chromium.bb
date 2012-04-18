// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/bluetooth/bluetooth_service_record.h"

#include <string>

#include "chrome/common/libxml_utils.h"

namespace {

static const char* kAttributeNode = "attribute";
static const char* kIdAttribute = "id";
static const char* kSdpNameId = "0x0100";
static const char* kTextNode = "text";
static const char* kValueAttribute = "value";

bool advanceToTag(XmlReader* reader, const char* node_type) {
  do {
    if (!reader->Read())
      return false;
  } while (reader->NodeName() != node_type);
  return true;
}

}  // namespace

namespace chromeos {

BluetoothServiceRecord::BluetoothServiceRecord(const std::string& xml_data) {
  XmlReader reader;
  if (!reader.Load(xml_data))
    return;

  while (advanceToTag(&reader, kAttributeNode)) {
    std::string id;
    if (reader.NodeAttribute(kIdAttribute, &id)) {
      if (id == kSdpNameId) {
        if (advanceToTag(&reader, kTextNode)) {
          std::string value;
          if (reader.NodeAttribute(kValueAttribute, &value))
            name_ = value;
        }
      }
    }
    // We don't care about anything else here, so find the closing tag
    advanceToTag(&reader, kAttributeNode);
  }
}

}  // namespace chromeos
