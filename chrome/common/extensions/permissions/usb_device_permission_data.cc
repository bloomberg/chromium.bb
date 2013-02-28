// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/usb_device_permission_data.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/values.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "chrome/common/extensions/permissions/usb_device_permission.h"

namespace {

const char* kProductIdKey = "productId";
const char* kVendorIdKey = "vendorId";

}  // namespace

namespace extensions {

UsbDevicePermissionData::UsbDevicePermissionData()
  : vendor_id_(0), product_id_(0) {
}

UsbDevicePermissionData::UsbDevicePermissionData(uint16 vendor_id,
                                                 uint16 product_id)
  : vendor_id_(vendor_id), product_id_(product_id) {
}

bool UsbDevicePermissionData::Check(
    const APIPermission::CheckParam* param) const {
  if (!param)
    return false;
  const UsbDevicePermission::CheckParam& specific_param =
      *static_cast<const UsbDevicePermission::CheckParam*>(param);
  return vendor_id_ == specific_param.vendor_id &&
      product_id_ == specific_param.product_id;
}

scoped_ptr<base::Value> UsbDevicePermissionData::ToValue() const {
  base::DictionaryValue* result = new base::DictionaryValue();
  result->SetInteger(kVendorIdKey, vendor_id_);
  result->SetInteger(kProductIdKey, product_id_);
  return scoped_ptr<base::Value>(result);
}

bool UsbDevicePermissionData::FromValue(const base::Value* value) {
  if (!value)
    return false;

  const base::DictionaryValue* dict_value;
  if (!value->GetAsDictionary(&dict_value))
    return false;

  int temp;
  if (!dict_value->GetInteger(kVendorIdKey, &temp))
    return false;
  if (temp < 0 || temp > kuint16max)
    return false;
  vendor_id_ = temp;

  if (!dict_value->GetInteger(kProductIdKey, &temp))
    return false;
  if (temp < 0 || temp > kuint16max)
    return false;
  product_id_ = temp;

  return true;
}

bool UsbDevicePermissionData::operator<(
    const UsbDevicePermissionData& rhs) const {
  if (vendor_id_ == rhs.vendor_id_)
    return product_id_ < rhs.product_id_;
  return vendor_id_ < rhs.vendor_id_;
}

bool UsbDevicePermissionData::operator==(
    const UsbDevicePermissionData& rhs) const {
  return vendor_id_ == rhs.vendor_id_ && product_id_ == rhs.product_id_;
}

}  // namespace extensions
