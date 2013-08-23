// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_TETHERING_ADB_FILTER_H_
#define CHROME_BROWSER_DEVTOOLS_TETHERING_ADB_FILTER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/ref_counted.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/devtools/adb_web_socket.h"
#include "chrome/browser/devtools/devtools_adb_bridge.h"

class PrefService;

namespace base {
class MessageLoop;
}

class TetheringAdbFilter : public base::RefCountedThreadSafe<
    TetheringAdbFilter,
    content::BrowserThread::DeleteOnUIThread> {
 public:
  typedef DevToolsAdbBridge::RemoteDevice::PortStatus PortStatus;
  typedef DevToolsAdbBridge::RemoteDevice::PortStatusMap PortStatusMap;

  TetheringAdbFilter(scoped_refptr<DevToolsAdbBridge::AndroidDevice> device,
                     base::MessageLoop* adb_message_loop,
                     PrefService* pref_service,
                     scoped_refptr<AdbWebSocket> web_socket);

  const PortStatusMap& GetPortStatusMap();

  bool ProcessIncomingMessage(const std::string& message);

 private:
  friend struct content::BrowserThread::DeleteOnThread<
      content::BrowserThread::UI>;
  friend class base::DeleteHelper<TetheringAdbFilter>;

  virtual ~TetheringAdbFilter();

  typedef std::map<int, std::string> ForwardingMap;

  typedef base::Callback<void(PortStatus)> CommandCallback;
  typedef std::map<int, CommandCallback> CommandCallbackMap;

  void OnPrefsChange();

  void ChangeForwardingMap(ForwardingMap map);

  void SerializeChanges(const std::string& method,
                        const ForwardingMap& old_map,
                        const ForwardingMap& new_map);

  void SendCommand(const std::string& method, int port);
  bool ProcessResponse(const std::string& json);

  void ProcessBindResponse(int port, PortStatus status);
  void ProcessUnbindResponse(int port, PortStatus status);
  void UpdateSocketCount(int port, int increment);
  void UpdatePortStatusMap();
  void UpdatePortStatusMapOnUIThread(const PortStatusMap& status_map);

  scoped_refptr<DevToolsAdbBridge::AndroidDevice> device_;
  base::MessageLoop* adb_message_loop_;
  PrefChangeRegistrar pref_change_registrar_;
  scoped_refptr<AdbWebSocket> web_socket_;
  int command_id_;
  ForwardingMap forwarding_map_;
  CommandCallbackMap pending_responses_;
  PortStatusMap port_status_;
  PortStatusMap port_status_on_ui_thread_;

  DISALLOW_COPY_AND_ASSIGN(TetheringAdbFilter);
};

class PortForwardingController {
 public:
  PortForwardingController(
      scoped_refptr<DevToolsAdbBridge> bridge,
      PrefService* pref_service);

  virtual ~PortForwardingController();

  void UpdateDeviceList(const DevToolsAdbBridge::RemoteDevices& devices);

 private:
  class Connection;
  typedef std::map<std::string, Connection*> Registry;

  scoped_refptr<DevToolsAdbBridge> bridge_;
  PrefService* pref_service_;
  Registry registry_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_TETHERING_ADB_FILTER_H_
