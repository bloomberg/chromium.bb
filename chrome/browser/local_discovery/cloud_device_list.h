// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_LOCAL_DISCOVERY_CLOUD_DEVICE_LIST_H_
#define CHROME_BROWSER_LOCAL_DISCOVERY_CLOUD_DEVICE_LIST_H_

#include <string>
#include <vector>

#include "chrome/browser/local_discovery/cloud_device_list_delegate.h"
#include "chrome/browser/local_discovery/gcd_base_api_flow.h"

namespace local_discovery {

class CloudDeviceList : public GCDBaseApiFlow::Delegate {
 public:
  typedef std::vector<CloudDeviceListDelegate::Device> DeviceList;
  typedef DeviceList::const_iterator iterator;

  CloudDeviceList(net::URLRequestContextGetter* request_context,
                  OAuth2TokenService* token_service,
                  const std::string& account_id,
                  CloudDeviceListDelegate* delegate);
  virtual ~CloudDeviceList();

  void Start();

  virtual void OnGCDAPIFlowError(GCDBaseApiFlow* flow,
                                 GCDBaseApiFlow::Status status) OVERRIDE;

  virtual void OnGCDAPIFlowComplete(
      GCDBaseApiFlow* flow,
      const base::DictionaryValue* value) OVERRIDE;

  virtual bool GCDIsCloudPrint() OVERRIDE;

  GCDBaseApiFlow* GetOAuth2ApiFlowForTests() { return &api_flow_; }

  const DeviceList& device_list() const {
    return device_list_;
  }

 private:
  bool FillDeviceDetails(const base::DictionaryValue* value,
                         CloudDeviceListDelegate::Device* device);

  scoped_refptr<net::URLRequestContextGetter> request_context_;
  DeviceList device_list_;
  CloudDeviceListDelegate* delegate_;
  GCDBaseApiFlow api_flow_;
};

}  // namespace local_discovery

#endif  // CHROME_BROWSER_LOCAL_DISCOVERY_CLOUD_DEVICE_LIST_H_
