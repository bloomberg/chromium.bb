// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_PORT_FORWARDING_CONTROLLER_H_
#define CHROME_BROWSER_DEVTOOLS_PORT_FORWARDING_CONTROLLER_H_

#include <map>

#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/devtools/devtools_adb_bridge.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;
class Profile;

class PortForwardingController : public KeyedService {
 public:
  explicit PortForwardingController(Profile* profile);

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
    virtual KeyedService* BuildServiceInstanceFor(
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

  void OnPrefsChange();
  bool ShouldCreateConnections();
  void ShutdownConnections();

  Profile* profile_;
  PrefService* pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
  Registry registry_;

  DISALLOW_COPY_AND_ASSIGN(PortForwardingController);
};

#endif  // CHROME_BROWSER_DEVTOOLS_PORT_FORWARDING_CONTROLLER_H_
