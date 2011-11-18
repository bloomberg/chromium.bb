// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_EVENT_ROUTER_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "ipc/ipc_message.h"

class GURL;
class Extension;
class ExtensionDevToolsManager;
class Profile;

namespace content {
class RenderProcessHost;
}

class ExtensionEventRouter : public content::NotificationObserver {
 public:
  // Sends an event via ipc_sender to the given extension. Can be called on
  // any thread.
  static void DispatchEvent(IPC::Message::Sender* ipc_sender,
                            const std::string& extension_id,
                            const std::string& event_name,
                            const std::string& event_args,
                            const GURL& event_url);

  explicit ExtensionEventRouter(Profile* profile);
  virtual ~ExtensionEventRouter();

  // Add or remove the process/extension pair as a listener for |event_name|.
  // Note that multiple extensions can share a process due to process
  // collapsing. Also, a single extension can have 2 processes if it is a split
  // mode extension.
  void AddEventListener(const std::string& event_name,
                        content::RenderProcessHost* process,
                        const std::string& extension_id);
  void RemoveEventListener(const std::string& event_name,
                           content::RenderProcessHost* process,
                           const std::string& extension_id);

  // Returns true if there is at least one listener for the given event.
  bool HasEventListener(const std::string& event_name);

  // Returns true if the extension is listening to the given event.
  bool ExtensionHasEventListener(const std::string& extension_id,
                                 const std::string& event_name);

  // Send an event to every registered extension renderer. If
  // |restrict_to_profile| is non-NULL, then the event will not be sent to other
  // profiles unless the extension has permission (e.g. incognito tab update ->
  // normal profile only works if extension is allowed incognito access). If
  // |event_url| is not empty, the event is only sent to extension with host
  // permissions for this url.
  void DispatchEventToRenderers(
      const std::string& event_name,
      const std::string& event_args,
      Profile* restrict_to_profile,
      const GURL& event_url);

  // Same as above, except only send the event to the given extension.
  virtual void DispatchEventToExtension(
      const std::string& extension_id,
      const std::string& event_name,
      const std::string& event_args,
      Profile* restrict_to_profile,
      const GURL& event_url);

  // Send different versions of an event to extensions in different profiles.
  // This is used in the case of sending one event to extensions that have
  // incognito access, and another event to extensions that don't (here),
  // in order to avoid sending 2 events to "spanning" extensions.
  // If |cross_incognito_profile| is non-NULL and different from
  // restrict_to_profile, send the event with cross_incognito_args to the
  // extensions in that profile that can't cross incognito.
  void DispatchEventsToRenderersAcrossIncognito(
      const std::string& event_name,
      const std::string& event_args,
      Profile* restrict_to_profile,
      const std::string& cross_incognito_args,
      const GURL& event_url);

  // Record the Event Ack from the renderer. (One less event in-flight.)
  void OnExtensionEventAck(const std::string& extension_id);

  // Check if there are any Extension Events that have not yet been acked by
  // the renderer.
  bool HasInFlightEvents(const std::string& extension_id);

 protected:
  // The details of an event to be dispatched.
  struct ExtensionEvent;

  // Shared by DispatchEvent*. If |extension_id| is empty, the event is
  // broadcast.
  // An event that just came off the pending list may not be delayed again.
  void DispatchEventImpl(const linked_ptr<ExtensionEvent>& event,
                         bool was_pending);

  // Dispatch may be delayed if the extension has a lazy background page.
  bool CanDispatchEventNow(const std::string& extension_id);

  // Store the event so that it can be dispatched (in order received)
  // when the background page is done loading.
  void AppendEvent(const linked_ptr<ExtensionEvent>& event);
  void DispatchPendingEvents(const std::string& extension_id);

 private:
  // An extension listening to an event.
  struct EventListener;

  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  Profile* profile_;

  content::NotificationRegistrar registrar_;

  scoped_refptr<ExtensionDevToolsManager> extension_devtools_manager_;

  // A map between an event name and a set of extensions that are listening
  // to that event.
  typedef std::map<std::string, std::set<EventListener> > ListenerMap;
  ListenerMap listeners_;

  // A map between an extension id and the queue of events pending
  // the load of it's background page.
  typedef std::vector<linked_ptr<ExtensionEvent> > PendingEventsList;
  typedef std::map<std::string,
                   linked_ptr<PendingEventsList> > PendingEventsPerExtMap;
  PendingEventsPerExtMap pending_events_;

  // Track of the number of dispatched events that have not yet sent an
  // ACK from the renderer.
  void IncrementInFlightEvents(const std::string& extension_id);
  std::map<std::string, int> in_flight_events_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionEventRouter);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_EVENT_ROUTER_H_
