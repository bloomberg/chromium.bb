// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_SERVICE_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/memory/ref_counted.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"
#include "ipc/ipc_message.h"

class GURL;
class Profile;
class TabContents;

// This class manages message and event passing between renderer processes.
// It maintains a list of processes that are listening to events and a set of
// open channels.
//
// Messaging works this way:
// - An extension-owned script context (like a toolstrip or a content script)
// adds an event listener to the "onConnect" event.
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
// port: an IPC::Message::Sender interface and an optional routing_id (in the
// case that the port is a tab).  The Sender is usually either a
// RenderProcessHost or a RenderViewHost.

// TODO(mpcomplete): Remove refcounting and make Profile the sole owner of this
// class. Then we can get rid of ProfileDestroyed().
class ExtensionMessageService
    : public base::RefCounted<ExtensionMessageService>,
      public NotificationObserver {
 public:
  // A messaging channel. Note that the opening port can be the same as the
  // receiver, if an extension toolstrip wants to talk to its tab (for example).
  struct MessageChannel;
  struct MessagePort;

  // Javascript function name constants.
  static const char kDispatchOnConnect[];
  static const char kDispatchOnDisconnect[];
  static const char kDispatchOnMessage[];

  // Allocates a pair of port ids.
  // NOTE: this can be called from any thread.
  static void AllocatePortIdPair(int* port1, int* port2);

  explicit ExtensionMessageService(Profile* profile);

  // Notification that our owning profile is going away.
  void DestroyingProfile();

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

  // Given an extension ID, opens a channel between the given
  // automation "port" or DevTools service and that extension. the
  // channel will be open to the extension process hosting the
  // background page and tool strip.
  //
  // Returns a port ID to be used for posting messages between the
  // processes, or -1 if the extension doesn't exist.
  int OpenSpecialChannelToExtension(
      const std::string& extension_id, const std::string& channel_name,
      const std::string& tab_json, IPC::Message::Sender* source);

  // Given an extension ID, opens a channel between the given DevTools
  // service and the content script for that extension running in the
  // designated tab.
  //
  // Returns a port ID identifying the DevTools end of the channel, to
  // be used for posting messages. May return -1 on failure, although
  // the code doesn't detect whether the extension actually exists.
  int OpenSpecialChannelToTab(
      const std::string& extension_id, const std::string& channel_name,
      TabContents* target_tab_contents, IPC::Message::Sender* source);

  // Closes the message channel associated with the given port, and notifies
  // the other side.
  void CloseChannel(int port_id);

  // Sends a message from a renderer to the given port.
  void PostMessageFromRenderer(int port_id, const std::string& message);

 private:
  friend class base::RefCounted<ExtensionMessageService>;
  friend class MockExtensionMessageService;

  // A map of channel ID to its channel object.
  typedef std::map<int, MessageChannel*> MessageChannelMap;

  virtual ~ExtensionMessageService();

  // Common among Open(Special)Channel* variants.
  bool OpenChannelImpl(
      IPC::Message::Sender* source,
      const std::string& tab_json,
      const MessagePort& receiver, int receiver_port_id,
      const std::string& source_extension_id,
      const std::string& target_extension_id,
      const std::string& channel_name);

  void CloseChannelImpl(MessageChannelMap::iterator channel_iter, int port_id,
                        bool notify_other_port);

  // NotificationObserver interface.
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // An IPC sender that might be in our list of channels has closed.
  void OnSenderClosed(IPC::Message::Sender* sender);

  Profile* profile_;

  NotificationRegistrar registrar_;

  MessageChannelMap channels_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionMessageService);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_MESSAGE_SERVICE_H_
