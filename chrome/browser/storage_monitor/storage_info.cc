// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/storage_monitor/storage_info.h"

#include "base/logging.h"

namespace {

// Prefix constants for different device id spaces.
const char kRemovableMassStorageWithDCIMPrefix[] = "dcim:";
const char kRemovableMassStorageNoDCIMPrefix[] = "nodcim:";
const char kFixedMassStoragePrefix[] = "path:";
const char kMtpPtpPrefix[] = "mtp:";
const char kMacImageCapturePrefix[] = "ic:";
const char kITunesPrefix[] = "itunes:";
const char kPicasaPrefix[] = "picasa:";

}  // namespace

StorageInfo::StorageInfo() : total_size_in_bytes_(0) {
}

StorageInfo::StorageInfo(const std::string& device_id_in,
                         const string16& device_name,
                         const base::FilePath::StringType& device_location,
                         const string16& label,
                         const string16& vendor,
                         const string16& model,
                         uint64 size_in_bytes)
    : device_id_(device_id_in),
      name_(device_name),
      location_(device_location),
      storage_label_(label),
      vendor_name_(vendor),
      model_name_(model),
      total_size_in_bytes_(size_in_bytes) {
}

StorageInfo::~StorageInfo() {
}

// static
std::string StorageInfo::MakeDeviceId(Type type, const std::string& unique_id) {
  DCHECK(!unique_id.empty());
  switch (type) {
    case REMOVABLE_MASS_STORAGE_WITH_DCIM:
      return std::string(kRemovableMassStorageWithDCIMPrefix) + unique_id;
    case REMOVABLE_MASS_STORAGE_NO_DCIM:
      return std::string(kRemovableMassStorageNoDCIMPrefix) + unique_id;
    case FIXED_MASS_STORAGE:
      return std::string(kFixedMassStoragePrefix) + unique_id;
    case MTP_OR_PTP:
      return std::string(kMtpPtpPrefix) + unique_id;
    case MAC_IMAGE_CAPTURE:
      return std::string(kMacImageCapturePrefix) + unique_id;
    case ITUNES:
      return std::string(kITunesPrefix) + unique_id;
    case PICASA:
      return std::string(kPicasaPrefix) + unique_id;
  }
  NOTREACHED();
  return std::string();
}

// static
bool StorageInfo::CrackDeviceId(const std::string& device_id,
                                Type* type, std::string* unique_id) {
  size_t prefix_length = device_id.find_first_of(':');
  std::string prefix = prefix_length != std::string::npos
                           ? device_id.substr(0, prefix_length + 1)
                           : std::string();

  Type found_type;
  if (prefix == kRemovableMassStorageWithDCIMPrefix) {
    found_type = REMOVABLE_MASS_STORAGE_WITH_DCIM;
  } else if (prefix == kRemovableMassStorageNoDCIMPrefix) {
    found_type = REMOVABLE_MASS_STORAGE_NO_DCIM;
  } else if (prefix == kFixedMassStoragePrefix) {
    found_type = FIXED_MASS_STORAGE;
  } else if (prefix == kMtpPtpPrefix) {
    found_type = MTP_OR_PTP;
  } else if (prefix == kMacImageCapturePrefix) {
    found_type = MAC_IMAGE_CAPTURE;
  } else if (prefix == kITunesPrefix) {
    found_type = ITUNES;
  } else if (prefix == kPicasaPrefix) {
    found_type = PICASA;
  } else {
    NOTREACHED();
    return false;
  }
  if (type)
    *type = found_type;

  if (unique_id)
    *unique_id = device_id.substr(prefix_length + 1);
  return true;
}

// static
bool StorageInfo::IsMediaDevice(const std::string& device_id) {
  Type type;
  return CrackDeviceId(device_id, &type, NULL) &&
      (type == REMOVABLE_MASS_STORAGE_WITH_DCIM || type == MTP_OR_PTP ||
       type == MAC_IMAGE_CAPTURE);
}

// static
bool StorageInfo::IsRemovableDevice(const std::string& device_id) {
  Type type;
  return CrackDeviceId(device_id, &type, NULL) &&
         (type == REMOVABLE_MASS_STORAGE_WITH_DCIM ||
          type == REMOVABLE_MASS_STORAGE_NO_DCIM ||
          type == MTP_OR_PTP ||
          type == MAC_IMAGE_CAPTURE);
}

// static
bool StorageInfo::IsMassStorageDevice(const std::string& device_id) {
  Type type;
  return CrackDeviceId(device_id, &type, NULL) &&
         (type == REMOVABLE_MASS_STORAGE_WITH_DCIM ||
          type == REMOVABLE_MASS_STORAGE_NO_DCIM ||
          type == FIXED_MASS_STORAGE ||
          type == ITUNES ||
          type == PICASA);
}

// static
bool StorageInfo::IsITunesDevice(const std::string& device_id) {
  Type type;
  return CrackDeviceId(device_id, &type, NULL) && type == ITUNES;
}

// static
bool StorageInfo::IsPicasaDevice(const std::string& device_id) {
  Type type;
  return CrackDeviceId(device_id, &type, NULL) && type == PICASA;
}
