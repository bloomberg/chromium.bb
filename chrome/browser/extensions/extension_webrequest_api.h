// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBREQUEST_API_H_
#define CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBREQUEST_API_H_
#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/singleton.h"
#include "ipc/ipc_message.h"
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/profiles/profile.h"

class ExtensionEventRouterForwarder;
class GURL;

// This class observes network events and routes them to the appropriate
// extensions listening to those events. All methods must be called on the IO
// thread unless otherwise specified.
class ExtensionWebRequestEventRouter {
 public:
  struct RequestFilter;
  struct ExtraInfoSpec;

  static ExtensionWebRequestEventRouter* GetInstance();

  // Removes the listener for the given sub-event. Can be called on the UI
  // thread.
  static void RemoveEventListenerOnUIThread(
      const std::string& extension_id,
      const std::string& sub_event_name);

  // TODO(mpcomplete): additional params
  void OnBeforeRequest(
      ExtensionEventRouterForwarder* event_router,
      ProfileId profile_id,
      const GURL& url,
      const std::string& method);

  // Adds a listener to the given event. |event_name| specifies the event being
  // listened to. |sub_event_name| is an internal event uniquely generated in
  // the extension process to correspond to the given filter and
  // extra_info_spec.
  void AddEventListener(
      const std::string& extension_id,
      const std::string& event_name,
      const std::string& sub_event_name,
      const RequestFilter& filter,
      int extra_info_spec);

  // Removes the listener for the given sub-event.
  void RemoveEventListener(
      const std::string& extension_id,
      const std::string& sub_event_name);

 private:
  friend struct DefaultSingletonTraits<ExtensionWebRequestEventRouter>;
  struct EventListener;
  typedef std::map<std::string, std::set<EventListener> > ListenerMap;

  ExtensionWebRequestEventRouter();
  ~ExtensionWebRequestEventRouter();

  // Returns a list of event listeners that care about the given event, based
  // on their filter parameters.
  std::vector<const EventListener*> GetMatchingListeners(
      const std::string& event_name, const GURL& url);

  // A map between an event name and a set of extensions that are listening
  // to that event.
  ListenerMap listeners_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionWebRequestEventRouter);
};

class WebRequestAddEventListener : public SyncExtensionFunction {
 public:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.webRequest.addEventListener");
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBREQUEST_API_H_
