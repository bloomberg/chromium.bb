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
#include "chrome/browser/extensions/extension_function.h"
#include "chrome/browser/profiles/profile.h"
#include "ipc/ipc_message.h"
#include "net/base/completion_callback.h"

class ExtensionEventRouterForwarder;
class GURL;

namespace net {
class URLRequest;
}

// This class observes network events and routes them to the appropriate
// extensions listening to those events. All methods must be called on the IO
// thread unless otherwise specified.
class ExtensionWebRequestEventRouter {
 public:
  struct RequestFilter;
  struct ExtraInfoSpec;

  static ExtensionWebRequestEventRouter* GetInstance();

  // Dispatches the OnBeforeRequest event to any extensions whose filters match
  // the given request. Returns true if an extension wants to pause the request.
  bool OnBeforeRequest(ProfileId profile_id,
                       ExtensionEventRouterForwarder* event_router,
                       net::URLRequest* request,
                       net::CompletionCallback* callback);

  // Called when an event listener handles a blocking event and responds.
  // TODO(mpcomplete): modify request
  void OnEventHandled(
      ProfileId profile_id,
      const std::string& extension_id,
      const std::string& event_name,
      const std::string& sub_event_name,
      uint64 request_id,
      bool cancel);

  // Adds a listener to the given event. |event_name| specifies the event being
  // listened to. |sub_event_name| is an internal event uniquely generated in
  // the extension process to correspond to the given filter and
  // extra_info_spec.
  void AddEventListener(
      ProfileId profile_id,
      const std::string& extension_id,
      const std::string& event_name,
      const std::string& sub_event_name,
      const RequestFilter& filter,
      int extra_info_spec);

  // Removes the listener for the given sub-event.
  void RemoveEventListener(
      ProfileId profile_id,
      const std::string& extension_id,
      const std::string& sub_event_name);

 private:
  friend struct DefaultSingletonTraits<ExtensionWebRequestEventRouter>;
  struct EventListener;
  struct BlockedRequest;
  typedef std::map<std::string, std::set<EventListener> > ListenerMapForProfile;
  typedef std::map<ProfileId, ListenerMapForProfile> ListenerMap;
  typedef std::map<uint64, BlockedRequest> BlockedRequestMap;

  ExtensionWebRequestEventRouter();
  ~ExtensionWebRequestEventRouter();

  // Returns a list of event listeners that care about the given event, based
  // on their filter parameters.
  std::vector<const EventListener*> GetMatchingListeners(
      ProfileId profile_id, const std::string& event_name, const GURL& url);

  // Decrements the count of event handlers blocking the given request. When the
  // count reaches 0 (or immediately if the request is being cancelled), we
  // stop blocking the request and either resume or cancel it.
  void DecrementBlockCount(uint64 request_id, bool cancel);

  // A map for each profile that maps an event name to a set of extensions that
  // are listening to that event.
  ListenerMap listeners_;

  // A map of network requests that are waiting for at least one event handler
  // to respond.
  BlockedRequestMap blocked_requests_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionWebRequestEventRouter);
};

class WebRequestAddEventListener : public SyncExtensionFunction {
 public:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.webRequest.addEventListener");
};

class WebRequestEventHandled : public SyncExtensionFunction {
 public:
  virtual bool RunImpl();
  DECLARE_EXTENSION_FUNCTION_NAME("experimental.webRequest.eventHandled");
};

#endif  // CHROME_BROWSER_EXTENSIONS_EXTENSION_WEBREQUEST_API_H_
