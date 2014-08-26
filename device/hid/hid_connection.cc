// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "device/hid/hid_connection.h"

#include <algorithm>

namespace device {

namespace {

// Functor used to filter collections by report ID.
struct CollectionHasReportId {
  explicit CollectionHasReportId(uint8_t report_id) : report_id_(report_id) {}

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
                              uint8_t report_id,
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

bool HasProtectedCollection(const HidDeviceInfo& device_info) {
  return std::find_if(device_info.collections.begin(),
                      device_info.collections.end(),
                      CollectionIsProtected()) != device_info.collections.end();
}

}  // namespace

HidConnection::HidConnection(const HidDeviceInfo& device_info)
    : device_info_(device_info) {
  has_protected_collection_ = HasProtectedCollection(device_info);
}

HidConnection::~HidConnection() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void HidConnection::Read(const ReadCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_info_.max_input_report_size == 0) {
    VLOG(1) << "This device does not support input reports.";
    callback.Run(false, NULL, 0);
    return;
  }

  PlatformRead(callback);
}

void HidConnection::Write(scoped_refptr<net::IOBuffer> buffer,
                          size_t size,
                          const WriteCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_info_.max_output_report_size == 0) {
    VLOG(1) << "This device does not support output reports.";
    callback.Run(false);
    return;
  }
  DCHECK_GE(size, 1u);
  uint8_t report_id = buffer->data()[0];
  if (device_info().has_report_id != (report_id != 0)) {
    VLOG(1) << "Invalid output report ID.";
    callback.Run(false);
    return;
  }
  if (IsReportIdProtected(report_id)) {
    VLOG(1) << "Attempt to set a protected output report.";
    callback.Run(false);
    return;
  }

  PlatformWrite(buffer, size, callback);
}

void HidConnection::GetFeatureReport(uint8_t report_id,
                                     const ReadCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_info_.max_feature_report_size == 0) {
    VLOG(1) << "This device does not support feature reports.";
    callback.Run(false, NULL, 0);
    return;
  }
  if (device_info().has_report_id != (report_id != 0)) {
    VLOG(1) << "Invalid feature report ID.";
    callback.Run(false, NULL, 0);
    return;
  }
  if (IsReportIdProtected(report_id)) {
    VLOG(1) << "Attempt to get a protected feature report.";
    callback.Run(false, NULL, 0);
    return;
  }

  PlatformGetFeatureReport(report_id, callback);
}

void HidConnection::SendFeatureReport(scoped_refptr<net::IOBuffer> buffer,
                                      size_t size,
                                      const WriteCallback& callback) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (device_info_.max_feature_report_size == 0) {
    VLOG(1) << "This device does not support feature reports.";
    callback.Run(false);
    return;
  }
  DCHECK_GE(size, 1u);
  uint8_t report_id = buffer->data()[0];
  if (device_info().has_report_id != (report_id != 0)) {
    VLOG(1) << "Invalid feature report ID.";
    callback.Run(false);
    return;
  }
  if (IsReportIdProtected(report_id)) {
    VLOG(1) << "Attempt to set a protected feature report.";
    callback.Run(false);
    return;
  }

  PlatformSendFeatureReport(buffer, size, callback);
}

bool HidConnection::CompleteRead(scoped_refptr<net::IOBuffer> buffer,
                                 size_t size,
                                 const ReadCallback& callback) {
  DCHECK_GE(size, 1u);
  uint8_t report_id = buffer->data()[0];
  if (IsReportIdProtected(report_id)) {
    VLOG(1) << "Filtered a protected input report.";
    return false;
  }

  callback.Run(true, buffer, size);
  return true;
}

bool HidConnection::IsReportIdProtected(uint8_t report_id) {
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
