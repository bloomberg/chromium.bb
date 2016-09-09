// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_ASCRIBER_H_
#define COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_ASCRIBER_H_

namespace net {
class URLRequest;
}

namespace data_use_measurement {

class DataUseRecorder;

// Abstract class that manages instances of DataUseRecorder and maps
// a URLRequest instance to its appropriate DataUseRecorder. An embedder
// should provide an override if it is interested in tracking data usage. Data
// use from all URLRequests mapped to the same DataUseRecorder will be grouped
// together and reported as a single use.
class DataUseAscriber {
 public:
  virtual ~DataUseAscriber() {}

  // Returns the DataUseRecorder to which data usage for the given URL should
  // be ascribed. If no existing DataUseRecorder exists, a new one will be
  // created.
  virtual DataUseRecorder* GetDataUseRecorder(
      const net::URLRequest* request) = 0;
};

}  // namespace data_use_measurement

#endif  // COMPONENTS_DATA_USE_MEASUREMENT_CORE_DATA_USE_ASCRIBER_H_
