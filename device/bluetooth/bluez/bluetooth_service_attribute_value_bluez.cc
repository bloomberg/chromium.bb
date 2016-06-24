// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/bluetooth/bluez/bluetooth_service_attribute_value_bluez.h"

#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace bluez {

BluetoothServiceAttributeValueBlueZ::BluetoothServiceAttributeValueBlueZ(
    Type type,
    size_t size,
    std::unique_ptr<base::Value> value)
    : type_(type), size_(size), value_(std::move(value)) {
  CHECK_NE(type, SEQUENCE);
}

BluetoothServiceAttributeValueBlueZ::BluetoothServiceAttributeValueBlueZ(
    std::unique_ptr<Sequence> sequence)
    : type_(SEQUENCE),
      size_(sequence->size()),
      sequence_(std::move(sequence)) {}

BluetoothServiceAttributeValueBlueZ::BluetoothServiceAttributeValueBlueZ(
    const BluetoothServiceAttributeValueBlueZ& attribute) {
  this->type_ = attribute.type_;
  this->size_ = attribute.size_;

  if (attribute.type_ != SEQUENCE) {
    this->value_ = base::WrapUnique(attribute.value_->DeepCopy());
    return;
  }

  this->sequence_ = base::MakeUnique<Sequence>(*attribute.sequence_);
}

BluetoothServiceAttributeValueBlueZ BluetoothServiceAttributeValueBlueZ::
operator=(const BluetoothServiceAttributeValueBlueZ& attribute) {
  return BluetoothServiceAttributeValueBlueZ(attribute);
}

BluetoothServiceAttributeValueBlueZ::~BluetoothServiceAttributeValueBlueZ() {}

}  // namespace bluez
