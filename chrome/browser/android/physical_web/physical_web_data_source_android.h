// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_PHYSICAL_WEB_PHYSICAL_WEB_DATA_SOURCE_ANDROID_H_
#define CHROME_BROWSER_ANDROID_PHYSICAL_WEB_PHYSICAL_WEB_DATA_SOURCE_ANDROID_H_

#include "base/macros.h"
#include "components/physical_web/data_source/physical_web_data_source.h"

namespace base {
class ListValue;
}

class PhysicalWebDataSourceAndroid : public PhysicalWebDataSource {
 public:
  PhysicalWebDataSourceAndroid();
  ~PhysicalWebDataSourceAndroid() override;

  void StartDiscovery(bool network_request_enabled) override;
  void StopDiscovery() override;

  std::unique_ptr<base::ListValue> GetMetadata() override;
  bool HasUnresolvedDiscoveries() override;

  void RegisterListener(PhysicalWebListener* listener) override;
  void UnregisterListener(PhysicalWebListener* listener) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(PhysicalWebDataSourceAndroid);
};

#endif  // CHROME_BROWSER_ANDROID_PHYSICAL_WEB_PHYSICAL_WEB_DATA_SOURCE_ANDROID_H_
