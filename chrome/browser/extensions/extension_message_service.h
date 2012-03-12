// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_SERVICE_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace content {
class RenderProcessHost;
class WebContents;
}

// This class manages message and event passing between renderer processes.
// It maintains a list of processes that are listening to events and a set of
// open channels.
//
// Messaging works this way:
// - An extension-owned script context (like a background page or a content
//   script) adds an event listener to the "onConnect" event.
// - Another context calls "extension.connect()" to open a channel to the
// extension process, or an extension context calls "tabs.connect(tabId)" to
// open a channel to the content scripts for the given tab.  The EMS notifies
// the target process/tab, which then calls the onConnect event in every
// context owned by the connecting extension in that process/tab.
// - Once the channel is established, either side can call postMessage to send
// a message to the opposite side of the channel, which may have multiple
// listeners.
//
// Terminology:
// channel: connection between two ports
// port: an IPC::Message::Process interface and an optional routing_id (in the
// case that the port is a tab).  The Process is usually either a
// RenderProcessHost or a RenderViewHost.
class ExtensionMessageService : public content::NotificationObserver {
 public:
  // A messaging channel. Note that the opening port can be the same as the
  // receiver, if an extension background page wants to talk to its tab (for
  // example).
  struct MessageChannel;
  struct MessagePort;

  // Javascript function name constants.
  static const char kDispatchOnConnect[];
  static const char kDispatchOnDisconnect[];

  // Allocates a pair of port ids.
  // NOTE: this can be called from any thread.
  static void AllocatePortIdPair(int* port1, int* port2);

  explicit ExtensionMessageService(Profile* profile);
  virtual ~ExtensionMessageService();

  // Given an extension's ID, opens a channel between the given renderer "port"
  // and every listening context owned by that extension. |channel_name| is
  // an optional identifier for use by extension developers.
  void OpenChannelToExtension(
      int source_process_id, int source_routing_id, int receiver_port_id,
      const std::string& source_extension_id,
      const std::string& target_extension_id,
      const std::string& channel_name);

  // Same as above, but opens a channel to the tab with the given ID.  Messages
  // are restricted to that tab, so if there are multiple tabs in that process,
  // only the targeted tab will receive messages.
  void OpenChannelToTab(
      int source_process_id, int source_routing_id, int receiver_port_id,
      int tab_id, const std::string& extension_id,
      const std::string& channel_name);

  // Closes the message channel associated with the given port, and notifies
  // the other side.
  void CloseChannel(int port_id);

  // Sends a message from a renderer to the given port.
  void PostMessageFromRenderer(int port_id, const std::string& message);

 private:
  friend class MockExtensionMessageService;

  // A map of channel ID to its channel object.
  typedef std::map<int, MessageChannel*> MessageChannelMap;

  // Common among Open(Special)Channel* variants.
  bool OpenChannelImpl(
      content::RenderProcessHost* source,
      const std::string& tab_json,
      const MessagePort& receiver, int receiver_port_id,
      const std::string& source_extension_id,
      const std::string& target_extension_id,
      const std::string& channel_name);

  void CloseChannelImpl(MessageChannelMap::iterator channel_iter, int port_id,
                        bool notify_other_port);

  // content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // A process that might be in our list of channels has closed.
  void OnProcessClosed(content::RenderProcessHost* process);

  content::NotificationRegistrar registrar_;

  MessageChannelMap channels_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageService);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_SERVICE_H_
