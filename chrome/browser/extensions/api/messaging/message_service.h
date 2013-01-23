// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_MESSAGING_MESSAGE_SERVICE_H_
#define CHROME_BROWSER_EXTENSIONS_API_MESSAGING_MESSAGE_SERVICE_H_

#include <map>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/extensions/api/messaging/native_message_process_host.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

class Profile;

namespace content {
class RenderProcessHost;
class WebContents;
}

namespace extensions {
class ExtensionHost;
class LazyBackgroundTaskQueue;

// This class manages message and event passing between renderer processes.
// It maintains a list of processes that are listening to events and a set of
// open channels.
//
// Messaging works this way:
// - An extension-owned script context (like a background page or a content
//   script) adds an event listener to the "onConnect" event.
// - Another context calls "runtime.connect()" to open a channel to the
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
class MessageService : public content::NotificationObserver,
                       public NativeMessageProcessHost::Client {
 public:
  // A messaging channel. Note that the opening port can be the same as the
  // receiver, if an extension background page wants to talk to its tab (for
  // example).
  struct MessageChannel;

  // One side of the communication handled by extensions::MessageService.
  class MessagePort {
   public:
    virtual ~MessagePort() {}
    // Notify the port that the channel has been opened.
    virtual void DispatchOnConnect(int dest_port_id,
                                   const std::string& channel_name,
                                   const std::string& tab_json,
                                   const std::string& source_extension_id,
                                   const std::string& target_extension_id) {}

    // Notify the port that the channel has been closed.
    virtual void DispatchOnDisconnect(int source_port_id,
                                      bool connection_error) {}

    // Dispatch a message to this end of the communication.
    virtual void DispatchOnMessage(const std::string& message,
                                   int target_port_id) = 0;

    // MessagPorts that target extensions will need to adjust their keepalive
    // counts for their lazy background page.
    virtual void IncrementLazyKeepaliveCount() {}
    virtual void DecrementLazyKeepaliveCount() {}

    // Get the RenderProcessHost (if any) associated with the port.
    virtual content::RenderProcessHost* GetRenderProcessHost();

   protected:
    MessagePort() {}

   private:
    DISALLOW_COPY_AND_ASSIGN(MessagePort);
  };

  // Allocates a pair of port ids.
  // NOTE: this can be called from any thread.
  static void AllocatePortIdPair(int* port1, int* port2);

  explicit MessageService(LazyBackgroundTaskQueue* queue);
  virtual ~MessageService();

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

  void OpenChannelToNativeApp(
      int source_process_id,
      int source_routing_id,
      int receiver_port_id,
      const std::string& source_extension_id,
      const std::string& native_app_name);

  // Closes the message channel associated with the given port, and notifies
  // the other side.
  virtual void CloseChannel(int port_id, bool connection_error) OVERRIDE;

  // Sends a message to the given port.
  void PostMessage(int port_id, const std::string& message);

  // NativeMessageProcessHost::Client
  virtual void PostMessageFromNativeProcess(
      int port_id,
      const std::string& message) OVERRIDE;

 private:
  friend class MockMessageService;
  struct OpenChannelParams;

  // A map of channel ID to its channel object.
  typedef std::map<int, MessageChannel*> MessageChannelMap;

  // A map of channel ID to information about the extension that is waiting
  // for that channel to open. Used for lazy background pages.
  typedef std::string ExtensionID;
  typedef std::pair<Profile*, ExtensionID> PendingChannel;
  typedef std::map<int, PendingChannel> PendingChannelMap;

  // Common among OpenChannel* variants.
  bool OpenChannelImpl(scoped_ptr<OpenChannelParams> params);

  void CloseChannelImpl(MessageChannelMap::iterator channel_iter,
                        int port_id, bool connection_error,
                        bool notify_other_port);

  // Have MessageService take ownership of |channel|, and remove any pending
  // channels with the same id.
  void AddChannel(MessageChannel* channel, int receiver_port_id);

  // content::NotificationObserver interface.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // A process that might be in our list of channels has closed.
  void OnProcessClosed(content::RenderProcessHost* process);

  // Potentially registers a pending task with the LazyBackgroundTaskQueue
  // to open a channel. Returns true if a task was queued.
  // Takes ownership of |params| if true is returned.
  bool MaybeAddPendingOpenChannelTask(Profile* profile,
                                      OpenChannelParams* params);

  // Callbacks for LazyBackgroundTaskQueue tasks. The queue passes in an
  // ExtensionHost to its task callbacks, though some of our callbacks don't
  // use that argument.
  void PendingOpenChannel(scoped_ptr<OpenChannelParams> params,
                          int source_process_id,
                          extensions::ExtensionHost* host);
  void PendingCloseChannel(int port_id,
                           bool connection_error,
                           extensions::ExtensionHost* host) {
    if (host)
      CloseChannel(port_id, connection_error);
  }
  void PendingPostMessage(int port_id,
                          const std::string& message,
                          extensions::ExtensionHost* host) {
    if (host)
      PostMessage(port_id, message);
  }

  content::NotificationRegistrar registrar_;
  MessageChannelMap channels_;
  PendingChannelMap pending_channels_;

  // Weak pointer. Guaranteed to outlive this class.
  LazyBackgroundTaskQueue* lazy_background_task_queue_;

  base::WeakPtrFactory<MessageService> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(MessageService);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_MESSAGING_MESSAGE_SERVICE_H_
