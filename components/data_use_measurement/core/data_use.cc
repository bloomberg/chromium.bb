// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/data_use_measurement/core/data_use.h"

namespace data_use_measurement {

DataUse::DataUse(TrafficType traffic_type)
    : traffic_type_(traffic_type),
      total_bytes_sent_(0),
      total_bytes_received_(0) {}

DataUse::~DataUse() {}

void DataUse::IncrementTotalBytes(int64_t bytes_received, int64_t bytes_sent) {
  total_bytes_received_ += bytes_received;
  total_bytes_sent_ += bytes_sent;
}

void DataUse::MergeFrom(const DataUse& other) {
  // Traffic type need not be same while merging. One of the data use created
  // when mainframe is created could have UNKNOWN traffic type, and later merged
  // with the data use created for its mainframe request which could be
  // USER_TRAFFIC.
  total_bytes_sent_ += other.total_bytes_sent_;
  total_bytes_received_ += other.total_bytes_received_;
}

}  // namespace data_use_measurement
