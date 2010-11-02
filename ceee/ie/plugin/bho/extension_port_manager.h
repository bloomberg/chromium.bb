// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Extension port manager takes care of managing the state of
// connecting and connected ports.
#ifndef CEEE_IE_PLUGIN_BHO_EXTENSION_PORT_MANAGER_H_
#define CEEE_IE_PLUGIN_BHO_EXTENSION_PORT_MANAGER_H_

#include <atlbase.h>
#include <atlcom.h>
#include <map>
#include <string>
#include "base/values.h"

// Fwd.
class IContentScriptNativeApi;
class IChromeFrameHost;

// The extension port manager takes care of:
// - Managing the state of connecting and connected ports
// - Transforming connection and post requests to outgoing message traffic.
// - Processing incoming message traffic and routing it to the appropriate
//   Native API instances for handling.
class ExtensionPortManager {
 public:
  ExtensionPortManager();
  virtual ~ExtensionPortManager();

  void Initialize(IChromeFrameHost* host);

  // Cleanup ports on native API uninitialization.
  // @param instance the instance that's going away.
  void CloseAll(IContentScriptNativeApi* instance);

  // Request opening a chnnel to an extension.
  // @param instance the API instance that will be handling this connection.
  // @param extension the extension to connect to.
  // @param tab info on the tab we're initiating a connection from.
  // @param cookie an opaque cookie that will be handed back to the
  //      instance once the connection completes or fails.
  HRESULT OpenChannelToExtension(IContentScriptNativeApi* instance,
                                 const std::string& extension,
                                 const std::string& channel_name,
                                 Value* tab,
                                 int cookie);

  // Post a message on an open port.
  // @param port_id the port's ID.
  // @param message the message to send.
  HRESULT PostMessage(int port_id, const std::string& message);

  // Process an incoming automation port message from the host.
  // @param message the automation message to process.
  void OnPortMessage(BSTR message);

 private:
  virtual HRESULT PostMessageToHost(const std::string& message,
                                    const std::string& target);

  // Represents a connected port.
  struct ConnectedPort {
    CComPtr<IContentScriptNativeApi> instance;
  };
  typedef std::map<int, ConnectedPort> ConnectedPortMap;

  // Represents a connecting port.
  struct ConnectingPort {
    CComPtr<IContentScriptNativeApi> instance;
    int cookie;
  };
  typedef std::map<int, ConnectingPort> ConnectingPortMap;

  // Map from port_id to page api instance.
  ConnectedPortMap connected_ports_;

  // Map from connection_id to page api and callback instances.
  ConnectingPortMap connecting_ports_;

  // The next connection id we'll assign.
  int next_connection_id_;

  CComPtr<IChromeFrameHost> chrome_frame_host_;
};

#endif  // CEEE_IE_PLUGIN_BHO_EXTENSION_PORT_MANAGER_H_
