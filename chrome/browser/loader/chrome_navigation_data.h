// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOADER_CHROME_NAVIGATION_DATA_H_
#define CHROME_BROWSER_LOADER_CHROME_NAVIGATION_DATA_H_

#include <memory>

#include "base/macros.h"
#include "base/supports_user_data.h"
#include "components/data_reduction_proxy/core/browser/data_reduction_proxy_data.h"
#include "components/previews/core/previews_user_data.h"
#include "content/public/common/previews_state.h"

namespace base {
class Value;
}

namespace net {
class URLRequest;
}

class ChromeNavigationData : public base::SupportsUserData::Data {
 public:
  ChromeNavigationData();
  ~ChromeNavigationData() override;

  // Convert from/to a base::Value.
  base::Value ToValue();

  // Every time a ChromeNavigationData is constructed from a base::Value, a new
  // instance of it and all of its children is built. If ChromeNavigationData
  // becomes large or if many observers begin to use it, we should consider only
  // deserializing the relevant part of ChromeNavigationData, not the whole
  // object each time.
  // Serialization is needed to transmit this object using Mojo. Another
  // approach to consider would be to serialize the object before each mojo
  // function call and immediatly reconstruct the object after that. Currently,
  // ChromeNavigationData is serialized when it enters the content/ layer and
  // reconstructed when it goes out.
  explicit ChromeNavigationData(const base::Value& value);

  // Takes ownership of |data_reduction_proxy_data|.
  void SetDataReductionProxyData(
      std::unique_ptr<data_reduction_proxy::DataReductionProxyData>
          data_reduction_proxy_data) {
    data_reduction_proxy_data_ = std::move(data_reduction_proxy_data);
  }

  data_reduction_proxy::DataReductionProxyData* GetDataReductionProxyData()
      const {
    return data_reduction_proxy_data_.get();
  }

  void set_previews_user_data(
      std::unique_ptr<previews::PreviewsUserData> previews_user_data) {
    previews_user_data_ = std::move(previews_user_data);
  }

  previews::PreviewsUserData* previews_user_data() const {
    return previews_user_data_.get();
  }

  content::PreviewsState previews_state() { return previews_state_; }
  void set_previews_state(content::PreviewsState previews_state) {
    previews_state_ = previews_state;
  }

  static ChromeNavigationData* GetDataAndCreateIfNecessary(
      net::URLRequest* request);

 private:
  // Manages the lifetime of optional DataReductionProxy information.
  std::unique_ptr<data_reduction_proxy::DataReductionProxyData>
      data_reduction_proxy_data_;

  // Manages the lifetime of optional Previews information.
  std::unique_ptr<previews::PreviewsUserData> previews_user_data_;

  content::PreviewsState previews_state_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNavigationData);
};

#endif  // CHROME_BROWSER_LOADER_CHROME_NAVIGATION_DATA_H_
