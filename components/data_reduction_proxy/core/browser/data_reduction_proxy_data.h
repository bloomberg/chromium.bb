// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_DATA_H_
#define COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_DATA_H_

#include <memory>

#include "base/macros.h"
#include "base/supports_user_data.h"

namespace net {
class URLRequest;
}

namespace data_reduction_proxy {

// DataReductionProxy-related data that can be put into UserData or other
// storage vehicles to associate this data with the object that owns it.
class DataReductionProxyData : public base::SupportsUserData::Data {
 public:
  DataReductionProxyData();

  // Whether the DataReductionProxy was used for this request or navigation.
  bool used_data_reduction_proxy() { return used_data_reduction_proxy_; }
  void set_used_data_reduction_proxy(bool used_data_reduction_proxy) {
    used_data_reduction_proxy_ = used_data_reduction_proxy;
  }

  // Whether Lo-Fi was requested for this request or navigation.
  bool lofi_requested() { return lofi_requested_; }
  void set_lofi_requested(bool lofi_requested) {
    lofi_requested_ = lofi_requested;
  }

  // Returns the Data from the URLRequest's UserData.
  static DataReductionProxyData* GetData(const net::URLRequest& request);
  // Returns the Data for a given URLRequest. If there is currently no
  // DataReductionProxyData on URLRequest, it creates one, and adds it to the
  // URLRequest's UserData, and returns a raw pointer to the new instance.
  static DataReductionProxyData* GetDataAndCreateIfNecessary(
      net::URLRequest* request);

  // Create a brand new instance of DataReductionProxyData that could be used in
  // a different thread. Several of deep copies may occur per navigation, so
  // this is inexpensive.
  std::unique_ptr<DataReductionProxyData> DeepCopy() const;

 private:
  // Whether the DataReductionProxy was used for this request or navigation.
  bool used_data_reduction_proxy_;

  // Whether Lo-Fi was requested for this request or navigation.
  bool lofi_requested_;

  DISALLOW_COPY_AND_ASSIGN(DataReductionProxyData);
};

}  // namespace data_reduction_proxy

#endif  // COMPONENTS_DATA_REDUCTION_PROXY_CORE_BROWSER_DATA_REDUCTION_PROXY_DATA_H_
