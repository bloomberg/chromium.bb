// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_connection.h"

#include <algorithm>

namespace device {

namespace {

// Functor used to filter collections by report ID.
struct CollectionHasReportId {
  explicit CollectionHasReportId(const uint8_t report_id)
      : report_id_(report_id) {}

  bool operator()(const HidCollectionInfo& info) const {
    if (info.report_ids.size() == 0 ||
        report_id_ == HidConnection::kNullReportId)
      return false;

    if (report_id_ == HidConnection::kAnyReportId)
      return true;

    return std::find(info.report_ids.begin(),
                     info.report_ids.end(),
                     report_id_) != info.report_ids.end();
  }

 private:
  const uint8_t report_id_;
};

// Functor returning true if collection has a protected usage.
struct CollectionIsProtected {
  bool operator()(const HidCollectionInfo& info) const {
    return info.usage.IsProtected();
  }
};

bool FindCollectionByReportId(const HidDeviceInfo& device_info,
                              const uint8_t report_id,
                              HidCollectionInfo* collection_info) {
  std::vector<HidCollectionInfo>::const_iterator collection_iter =
      std::find_if(device_info.collections.begin(),
                   device_info.collections.end(),
                   CollectionHasReportId(report_id));
  if (collection_iter != device_info.collections.end()) {
    if (collection_info) {
      *collection_info = *collection_iter;
    }
    return true;
  }

  return false;
}

bool HasReportId(const HidDeviceInfo& device_info) {
  return FindCollectionByReportId(
      device_info, HidConnection::kAnyReportId, NULL);
}

bool HasProtectedCollection(const HidDeviceInfo& device_info) {
  return std::find_if(device_info.collections.begin(),
                      device_info.collections.end(),
                      CollectionIsProtected()) != device_info.collections.end();
}

}  // namespace

HidConnection::HidConnection(const HidDeviceInfo& device_info)
    : device_info_(device_info) {
  has_protected_collection_ = HasProtectedCollection(device_info);
  has_report_id_ = HasReportId(device_info);
}

HidConnection::~HidConnection() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void HidConnection::Read(scoped_refptr<net::IOBufferWithSize> buffer,
                         const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_info_.max_input_report_size == 0) {
    // The device does not support input reports.
    callback.Run(false, 0);
    return;
  }
  int expected_buffer_size = device_info_.max_input_report_size;
  if (!has_report_id()) {
    expected_buffer_size--;
  }
  if (buffer->size() < expected_buffer_size) {
    // Receive buffer is too small.
    callback.Run(false, 0);
    return;
  }

  PlatformRead(buffer, callback);
}

void HidConnection::Write(uint8_t report_id,
                          scoped_refptr<net::IOBufferWithSize> buffer,
                          const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_info_.max_output_report_size == 0) {
    // The device does not support output reports.
    callback.Run(false, 0);
    return;
  }
  if (IsReportIdProtected(report_id)) {
    callback.Run(false, 0);
    return;
  }

  PlatformWrite(report_id, buffer, callback);
}

void HidConnection::GetFeatureReport(
    uint8_t report_id,
    scoped_refptr<net::IOBufferWithSize> buffer,
    const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_info_.max_feature_report_size == 0) {
    // The device does not support feature reports.
    callback.Run(false, 0);
    return;
  }
  if (IsReportIdProtected(report_id)) {
    callback.Run(false, 0);
    return;
  }
  int expected_buffer_size = device_info_.max_feature_report_size;
  if (!has_report_id()) {
    expected_buffer_size--;
  }
  if (buffer->size() < expected_buffer_size) {
    // Receive buffer is too small.
    callback.Run(false, 0);
    return;
  }

  PlatformGetFeatureReport(report_id, buffer, callback);
}

void HidConnection::SendFeatureReport(
    uint8_t report_id,
    scoped_refptr<net::IOBufferWithSize> buffer,
    const IOCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_info_.max_feature_report_size == 0) {
    // The device does not support feature reports.
    callback.Run(false, 0);
    return;
  }
  if (IsReportIdProtected(report_id)) {
    callback.Run(false, 0);
    return;
  }

  PlatformSendFeatureReport(report_id, buffer, callback);
}

bool HidConnection::CompleteRead(scoped_refptr<net::IOBufferWithSize> buffer,
                                 const IOCallback& callback) {
  if (buffer->size() == 0 || IsReportIdProtected(buffer->data()[0])) {
    return false;
  }

  callback.Run(true, buffer->size());
  return true;
}

bool HidConnection::IsReportIdProtected(const uint8_t report_id) {
  HidCollectionInfo collection_info;
  if (FindCollectionByReportId(device_info_, report_id, &collection_info)) {
    return collection_info.usage.IsProtected();
  }

  return has_protected_collection();
}

PendingHidReport::PendingHidReport() {}

PendingHidReport::~PendingHidReport() {}

PendingHidRead::PendingHidRead() {}

PendingHidRead::~PendingHidRead() {}

}  // namespace device
