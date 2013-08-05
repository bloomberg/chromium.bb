// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DEVTOOLS_TETHERING_ADB_FILTER_H_
#define CHROME_BROWSER_DEVTOOLS_TETHERING_ADB_FILTER_H_

#include <map>
#include <string>

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/devtools/adb_web_socket.h"
#include "chrome/browser/devtools/devtools_adb_bridge.h"

class PrefService;

namespace base {
class MessageLoop;
}

class TetheringAdbFilter {
 public:
  TetheringAdbFilter(int adb_port,
                     const std::string& serial,
                     base::MessageLoop* adb_message_loop,
                     PrefService* pref_service,
                     scoped_refptr<AdbWebSocket> web_socket);
  ~TetheringAdbFilter();

  bool ProcessIncomingMessage(const std::string& message);

 private:
  typedef std::map<int, std::string> ForwardingMap;

  void OnPrefsChange();

  void ChangeForwardingMap(ForwardingMap map);

  void SerializeChanges(const std::string& method,
                        const ForwardingMap& old_map,
                        const ForwardingMap& new_map);

  void SendCommand(const std::string& method, int port);

  int adb_port_;
  std::string serial_;
  base::MessageLoop* adb_message_loop_;
  PrefChangeRegistrar pref_change_registrar_;
  scoped_refptr<AdbWebSocket> web_socket_;
  int command_id_;
  ForwardingMap forwarding_map_;
  base::WeakPtrFactory<TetheringAdbFilter> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TetheringAdbFilter);
};

class PortForwardingController {
 public:
  PortForwardingController(
      base::MessageLoop* adb_message_loop,
      PrefService* pref_service);

  virtual ~PortForwardingController();

  void UpdateDeviceList(const DevToolsAdbBridge::RemoteDevices& devices);

 private:
  class Connection;
  typedef std::map<std::string, Connection*> Registry;

  base::MessageLoop* adb_message_loop_;
  PrefService* pref_service_;
  Registry registry_;
};

#endif  // CHROME_BROWSER_DEVTOOLS_TETHERING_ADB_FILTER_H_
