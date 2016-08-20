// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_SAVER_STATUS_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_SAVER_STATUS_H_

namespace data_reduction_proxy {

// A class that reports the current state of data saver.
class DataSaverStatus {
 public:
  // Whether data saver is currently enabled.
  virtual bool IsDataSaverEnabled() const = 0;
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_COMMON_DATA_SAVER_STATUS_H_
