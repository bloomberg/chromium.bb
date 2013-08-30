// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PORT_FORWARDING_CONTROLLER_H_
#define CHROME_BROWSER_DEVTOOLS_PORT_FORWARDING_CONTROLLER_H_

#include <map>

#include "chrome/browser/devtools/devtools_adb_bridge.h"

class PrefService;

class PortForwardingController {
 public:
  PortForwardingController(
      scoped_refptr<DevToolsAdbBridge> bridge,
      PrefService* pref_service);

  ~PortForwardingController();

  void UpdateDeviceList(const DevToolsAdbBridge::RemoteDevices& devices);

 private:
  class Connection;
  typedef std::map<std::string, Connection* > Registry;

  scoped_refptr<DevToolsAdbBridge> bridge_;
  PrefService* pref_service_;
  Registry registry_;

  DISALLOW_COPY_AND_ASSIGN(PortForwardingController);
};

#endif  // CHROME_BROWSER_DEVTOOLS_PORT_FORWARDING_CONTROLLER_H_
