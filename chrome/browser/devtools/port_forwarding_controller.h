// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PORT_FORWARDING_CONTROLLER_H_
#define CHROME_BROWSER_DEVTOOLS_PORT_FORWARDING_CONTROLLER_H_

#include <map>

#include "chrome/browser/devtools/devtools_adb_bridge.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service.h"
#include "components/browser_context_keyed_service/browser_context_keyed_service_factory.h"

class PrefService;

class PortForwardingController : public BrowserContextKeyedService {
 public:
  explicit PortForwardingController(PrefService* pref_service);

  virtual ~PortForwardingController();

  class Factory : public BrowserContextKeyedServiceFactory {
   public:
    // Returns singleton instance of Factory.
    static Factory* GetInstance();

    // Returns PortForwardingController associated with |profile|.
    static PortForwardingController* GetForProfile(Profile* profile);

   private:
    friend struct DefaultSingletonTraits<Factory>;

    Factory();
    virtual ~Factory();

    // BrowserContextKeyedServiceFactory overrides:
    virtual BrowserContextKeyedService* BuildServiceInstanceFor(
        content::BrowserContext* context) const OVERRIDE;
    DISALLOW_COPY_AND_ASSIGN(Factory);
  };

  typedef int PortStatus;
  typedef std::map<int, PortStatus> PortStatusMap;
  typedef std::map<std::string, PortStatusMap> DevicesStatus;

  DevicesStatus UpdateDeviceList(
      const DevToolsAdbBridge::RemoteDevices& devices);

 private:
  class Connection;
  typedef std::map<std::string, Connection* > Registry;

  scoped_refptr<DevToolsAdbBridge::RefCountedAdbThread> adb_thread_;
  PrefService* pref_service_;
  Registry registry_;

  DISALLOW_COPY_AND_ASSIGN(PortForwardingController);
};

#endif  // CHROME_BROWSER_DEVTOOLS_PORT_FORWARDING_CONTROLLER_H_
