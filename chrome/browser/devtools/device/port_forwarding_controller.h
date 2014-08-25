// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_DEVICE_PORT_FORWARDING_CONTROLLER_H_
#define CHROME_BROWSER_DEVTOOLS_DEVICE_PORT_FORWARDING_CONTROLLER_H_

#include <map>

#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/devtools/device/devtools_android_bridge.h"
#include "components/keyed_service/content/browser_context_keyed_service_factory.h"
#include "components/keyed_service/core/keyed_service.h"

class PrefService;
class Profile;

class PortForwardingController
    : private KeyedService,
      private DevToolsAndroidBridge::DeviceListListener {
 public:
  explicit PortForwardingController(Profile* profile);

  virtual ~PortForwardingController();

  // KeyedService implementation.
  virtual void Shutdown() OVERRIDE;

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

  class Listener {
   public:
    typedef PortForwardingController::PortStatusMap PortStatusMap;
    typedef PortForwardingController::DevicesStatus DevicesStatus;

    virtual void PortStatusChanged(const DevicesStatus&) = 0;
   protected:
    virtual ~Listener() {}
  };

  void AddListener(Listener* listener);
  void RemoveListener(Listener* listener);

 private:
  class Connection;
  typedef std::map<std::string, Connection*> Registry;

  // DevToolsAndroidBridge::Listener implementation.
  virtual void DeviceListChanged(
      const DevToolsAndroidBridge::RemoteDevices& devices) OVERRIDE;

  void OnPrefsChange();

  void StartListening();
  void StopListening();

  void UpdateConnections();
  void ShutdownConnections();

  void NotifyListeners(const DevicesStatus& status) const;

  Profile* profile_;
  PrefService* pref_service_;
  PrefChangeRegistrar pref_change_registrar_;
  Registry registry_;

  typedef std::vector<Listener*> Listeners;
  Listeners listeners_;
  bool listening_;

  typedef std::map<int, std::string> ForwardingMap;
  ForwardingMap forwarding_map_;

  DISALLOW_COPY_AND_ASSIGN(PortForwardingController);
};

#endif  // CHROME_BROWSER_DEVTOOLS_DEVICE_PORT_FORWARDING_CONTROLLER_H_
