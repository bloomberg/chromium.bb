// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_EVENT_ROUTER_FORWARDER_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_EVENT_ROUTER_FORWARDER_H_
#pragma once

#include <string>

#include "base/basictypes.h"
#include "base/ref_counted.h"
#include "chrome/browser/browser_thread.h"

class GURL;
class Profile;

// This class forwards events to ExtensionEventRouters.
// The advantages of this class over direct usage of ExtensionEventRouters are:
// - this class is thread-safe, you can call the functions from UI and IO
//   thread.
// - the class can handle if a profile is deleted between the time of sending
//   the event from the IO thread to the UI thread.
// - this class can be used in contexts that are not governed by a profile, e.g.
//   by system URLRequestContexts. In these cases the |restrict_to_profile|
//   parameter remains NULL and events are broadcasted to all profiles.
class ExtensionEventRouterForwarder
    : public base::RefCountedThreadSafe<ExtensionEventRouterForwarder> {
 public:
  ExtensionEventRouterForwarder();

  // Calls
  //   DispatchEventToRenderers(event_name, event_args, profile, event_url)
  // on all (original) profiles' ExtensionEventRouters.
  // May be called on any thread.
  void BroadcastEventToRenderers(const std::string& event_name,
                                 const std::string& event_args,
                                 const GURL& event_url);

  // Calls
  //   DispatchEventToExtension(extension_id, event_name, event_args,
  //       profile, event_url)
  // on all (original) profiles' ExtensionEventRouters.
  // May be called on any thread.
  void BroadcastEventToExtension(const std::string& extension_id,
                                 const std::string& event_name,
                                 const std::string& event_args,
                                 const GURL& event_url);

  // Calls
  //   DispatchEventToRenderers(event_name, event_args,
  //       use_profile_to_restrict_events ? profile : NULL, event_url)
  // on |profile|'s ExtensionEventRouter. May be called on any thread.
  void DispatchEventToRenderers(const std::string& event_name,
                                const std::string& event_args,
                                Profile* profile,
                                bool use_profile_to_restrict_events,
                                const GURL& event_url);

  // Calls
  //   DispatchEventToExtension(extension_id, event_name, event_args,
  //       use_profile_to_restrict_events ? profile : NULL, event_url)
  // on |profile|'s ExtensionEventRouter. May be called on any thread.
  void DispatchEventToExtension(const std::string& extension_id,
                                const std::string& event_name,
                                const std::string& event_args,
                                Profile* profile,
                                bool use_profile_to_restrict_events,
                                const GURL& event_url);

 protected:
  // Protected for testing.
  virtual ~ExtensionEventRouterForwarder();

  // Calls DispatchEventToRenderers or DispatchEventToExtension (depending on
  // whether extension_id == "" or not) of |profile|'s ExtensionEventRouter.
  // |profile| may never be NULL.
  // Virtual for testing.
  virtual void CallExtensionEventRouter(Profile* profile,
                                        const std::string& extension_id,
                                        const std::string& event_name,
                                        const std::string& event_args,
                                        Profile* restrict_to_profile,
                                        const GURL& event_url);

 private:
  friend class base::RefCountedThreadSafe<ExtensionEventRouterForwarder>;

  // Helper function for {Broadcast,Dispatch}EventTo{Extension,Renderers}.
  void HandleEvent(const std::string& extension_id,
                   const std::string& event_name,
                   const std::string& event_args,
                   Profile* profile,
                   bool use_profile_to_restrict_events,
                   const GURL& event_url);

  DISALLOW_COPY_AND_ASSIGN(ExtensionEventRouterForwarder);
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_EVENT_ROUTER_FORWARDER_H_
