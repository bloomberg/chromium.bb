// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_EVENT_ROUTER_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/ref_counted.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class GURL;
class Extension;
class ExtensionDevToolsManager;
class Profile;
class RenderProcessHost;

class ExtensionEventRouter : public NotificationObserver {
 public:
  // Returns true if the given extension can see events and data from another
  // sub-profile (incognito to original profile, or vice versa).
  static bool CanCrossIncognito(Profile* profile,
                                const std::string& extension_id);
  static bool CanCrossIncognito(Profile* profile, const Extension* extension);

  explicit ExtensionEventRouter(Profile* profile);
  ~ExtensionEventRouter();

  // Add or remove the process/extension pair as a listener for |event_name|.
  // Note that multiple extensions can share a process due to process
  // collapsing. Also, a single extension can have 2 processes if it is a split
  // mode extension.
  void AddEventListener(const std::string& event_name,
                        RenderProcessHost* process,
                        const std::string& extension_id);
  void RemoveEventListener(const std::string& event_name,
                           RenderProcessHost* process,
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
      const std::string& event_name, const std::string& event_args,
      Profile* restrict_to_profile, const GURL& event_url);

  // Same as above, except only send the event to the given extension.
  void DispatchEventToExtension(
      const std::string& extension_id,
      const std::string& event_name, const std::string& event_args,
      Profile* restrict_to_profile, const GURL& event_url);

 protected:
  // Shared by DispatchEvent*. If |extension_id| is empty, the event is
  // broadcast.
  virtual void DispatchEventImpl(
      const std::string& extension_id,
      const std::string& event_name, const std::string& event_args,
      Profile* restrict_to_profile, const GURL& event_url);

 private:
  // An extension listening to an event.
  struct EventListener;

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  Profile* profile_;

  NotificationRegistrar registrar_;

  scoped_refptr<ExtensionDevToolsManager> extension_devtools_manager_;

  // A map between an event name and a set of extensions that are listening
  // to that event.
  typedef std::map<std::string, std::set<EventListener> > ListenerMap;
  ListenerMap listeners_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionEventRouter);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_EVENT_ROUTER_H_
