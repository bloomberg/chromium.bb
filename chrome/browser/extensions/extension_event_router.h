// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_EVENT_ROUTER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_EVENT_ROUTER_H_
#pragma once

#include <map>
#include <set>
#include <string>

#include "base/ref_counted.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"

class GURL;
class ExtensionDevToolsManager;
class Profile;
class RenderProcessHost;

class ExtensionEventRouter : public NotificationObserver {
 public:
  explicit ExtensionEventRouter(Profile* profile);
  ~ExtensionEventRouter();

  // Returns the event name for an event that is extension-specific.
  static std::string GetPerExtensionEventName(const std::string& event_name,
                                              const std::string& extension_id);

  // Add or remove |render_process_id| as a listener for |event_name|.
  void AddEventListener(const std::string& event_name,
                        int render_process_id);
  void RemoveEventListener(const std::string& event_name,
                           int render_process_id);

  // Returns true if there is at least one listener for the given event.
  bool HasEventListener(const std::string& event_name);

  // Send an event to every registered extension renderer. If
  // |restrict_to_profile| is non-NULL, then the event will not be sent to other
  // profiles unless the extension has permission (e.g. incognito tab update ->
  // normal profile only works if extension is allowed incognito access). If
  // |event_url| is not empty, the event is only sent to extension with host
  // permissions for this url.
  virtual void DispatchEventToRenderers(
      const std::string& event_name, const std::string& event_args,
      Profile* restrict_to_profile, const GURL& event_url);

  // Same as above, except use the extension-specific naming scheme for the
  // event. This is used by events that are per-extension.
  void DispatchEventToExtension(
      const std::string& extension_id,
      const std::string& event_name, const std::string& event_args,
      Profile* restrict_to_profile, const GURL& event_url);

 private:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  Profile* profile_;

  NotificationRegistrar registrar_;

  scoped_refptr<ExtensionDevToolsManager> extension_devtools_manager_;

  // A map between an event name and a set of process id's that are listening
  // to that event.
  typedef std::map<std::string, std::set<int> > ListenerMap;
  ListenerMap listeners_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionEventRouter);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_EVENT_ROUTER_H_
