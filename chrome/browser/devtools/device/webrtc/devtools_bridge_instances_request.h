// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_WEBRTC_DEVTOOLS_BRIDGE_INSTANCES_REQUEST_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_WEBRTC_DEVTOOLS_BRIDGE_INSTANCES_REQUEST_H_

#include "chrome/browser/devtools/device/android_device_manager.h"
#include "chrome/browser/local_discovery/gcd_api_flow.h"

class DevToolsBridgeInstancesRequest
    : public local_discovery::GCDApiFlowRequest {
 public:
  struct Instance {
    std::string id;
    std::string display_name;

    ~Instance();
  };

  using InstanceList = std::vector<Instance>;

  class Delegate {
   public:
    virtual void OnDevToolsBridgeInstancesRequestSucceeded(
        const InstanceList& result) = 0;
    virtual void OnDevToolsBridgeInstancesRequestFailed() = 0;

   protected:
    ~Delegate() {}
  };

  explicit DevToolsBridgeInstancesRequest(Delegate* delegate);
  ~DevToolsBridgeInstancesRequest() override;

  // Implementation of GCDApiFlowRequest.
  void OnGCDAPIFlowError(local_discovery::GCDApiFlow::Status status) override;
  void OnGCDAPIFlowComplete(const base::DictionaryValue& value) override;
  GURL GetURL() override;

 private:
  void TryAddInstance(const base::DictionaryValue& device_value);

  Delegate* const delegate_;
  InstanceList result_;

  DISALLOW_COPY_AND_ASSIGN(DevToolsBridgeInstancesRequest);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_WEBRTC_DEVTOOLS_BRIDGE_INSTANCES_REQUEST_H_
