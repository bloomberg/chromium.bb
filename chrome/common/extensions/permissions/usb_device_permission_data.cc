// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/common/extensions/permissions/usb_device_permission_data.h"

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/string_number_conversions.h"
#include "base/string_split.h"
#include "chrome/common/extensions/permissions/api_permission.h"
#include "chrome/common/extensions/permissions/usb_device_permission.h"

namespace {

const char kColon = ':';

}  // namespace

namespace extensions {

UsbDevicePermissionData::UsbDevicePermissionData()
  : vendor_id_(0), product_id_(0), spec_("") {
}

UsbDevicePermissionData::UsbDevicePermissionData(uint16 vendor_id,
                                                 uint16 product_id)
  : vendor_id_(vendor_id), product_id_(product_id), spec_("") {
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

bool UsbDevicePermissionData::Parse(const std::string& spec) {
  spec_.clear();

  std::vector<std::string> tokens;
  base::SplitStringDontTrim(spec, kColon, &tokens);
  if (tokens.size() != 2)
    return false;

  int temp;
  if (!base::StringToInt(tokens[0], &temp) || temp < 0 || temp > kuint16max)
    return false;
  vendor_id_ = temp;

  if (!base::StringToInt(tokens[1], &temp) || temp < 0 || temp > kuint16max)
    return false;
  product_id_ = temp;

  return true;
}

const std::string& UsbDevicePermissionData::GetAsString() const {
  if (spec_.empty()) {
    spec_.append(base::IntToString(vendor_id_));
    spec_.append(1, kColon);
    spec_.append(base::IntToString(product_id_));
  }
  return spec_;
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
