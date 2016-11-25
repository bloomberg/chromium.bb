// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef IOS_CHROME_BROWSER_PHYSICAL_WEB_IOS_CHROME_PHYSICAL_WEB_DATA_SOURCE_H_
#define IOS_CHROME_BROWSER_PHYSICAL_WEB_IOS_CHROME_PHYSICAL_WEB_DATA_SOURCE_H_

#include "base/macros.h"
#import "base/mac/scoped_nsobject.h"
#include "components/physical_web/data_source/physical_web_data_source_impl.h"

namespace base {
class ListValue;
}
class PhysicalWebListener;

@class PhysicalWebScanner;

// iOS implementation of PhysicalWebDataSource
class IOSChromePhysicalWebDataSource
    : public physical_web::PhysicalWebDataSourceImpl {
 public:
  IOSChromePhysicalWebDataSource();
  ~IOSChromePhysicalWebDataSource() override;

  // Starts scanning for Physical Web URLs. If |network_request_enabled| is
  // true, discovered URLs will be sent to a resolution service.
  void StartDiscovery(bool network_request_enabled) override;

  // Stops scanning for Physical Web URLs and clears cached URL content.
  void StopDiscovery() override;

  // Returns a list of resolved URLs and associated page metadata. If network
  // requests are disabled, the list will be empty.
  std::unique_ptr<base::ListValue> GetMetadata() override;

  // Returns boolean |true| if network requests are disabled and there are one
  // or more discovered URLs that have not been sent to the resolution service.
  bool HasUnresolvedDiscoveries() override;

 private:
  // Scanner for nearby Physical Web URL devices.
  base::scoped_nsobject<PhysicalWebScanner> scanner_;

  DISALLOW_COPY_AND_ASSIGN(IOSChromePhysicalWebDataSource);
};

#endif  // IOS_CHROME_COMMON_PHYSICAL_WEB_IOS_CHROME_PHYSICAL_WEB_DATA_SOURCE_H_
