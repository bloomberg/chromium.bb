// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_CLOUD_DEVICE_LIST_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_CLOUD_DEVICE_LIST_H_

#include <string>
#include <vector>

#include "base/values.h"
#include "chrome/browser/local_discovery/cloud_device_list_delegate.h"
#include "chrome/browser/local_discovery/gcd_api_flow.h"

namespace local_discovery {

class CloudDeviceList : public GCDApiFlowRequest {
 public:
  typedef std::vector<CloudDeviceListDelegate::Device> DeviceList;
  typedef DeviceList::const_iterator iterator;

  explicit CloudDeviceList(CloudDeviceListDelegate* delegate);
  virtual ~CloudDeviceList();

  virtual void OnGCDAPIFlowError(GCDApiFlow::Status status) OVERRIDE;

  virtual void OnGCDAPIFlowComplete(
      const base::DictionaryValue& value) OVERRIDE;

  virtual GURL GetURL() OVERRIDE;

 private:
  bool FillDeviceDetails(const base::DictionaryValue& value,
                         CloudDeviceListDelegate::Device* device);

  CloudDeviceListDelegate* delegate_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_CLOUD_DEVICE_LIST_H_
