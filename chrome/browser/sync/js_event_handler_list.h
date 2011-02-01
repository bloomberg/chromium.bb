// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_JS_EVENT_HANDLER_LIST_H_
#define CHROME_BROWSER_SYNC_JS_EVENT_HANDLER_LIST_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/observer_list.h"
#include "chrome/browser/sync/js_arg_list.h"
#include "chrome/browser/sync/js_event_router.h"
#include "chrome/browser/sync/js_frontend.h"

namespace browser_sync {

class JsBackend;
class JsEventHandler;

// A beefed-up ObserverList<JsEventHandler> that transparently handles
// the communication between the handlers and a backend.
class JsEventHandlerList : public JsFrontend, public JsEventRouter {
 public:
  JsEventHandlerList();

  virtual ~JsEventHandlerList();

  // Sets the backend to route all messages to.  Should be called only
  // if a backend has not already been set.
  void SetBackend(JsBackend* backend);

  // Removes any existing backend.
  void RemoveBackend();

  // JsFrontend implementation.  Routes messages to any attached
  // backend; if there is none, queues up the message for processing
  // when the next backend is attached.
  virtual void AddHandler(JsEventHandler* handler);
  virtual void RemoveHandler(JsEventHandler* handler);
  virtual void ProcessMessage(
      const std::string& name, const JsArgList& args,
      const JsEventHandler* sender);

  // JsEventRouter implementation.  Routes the event to the
  // appropriate handler(s).
  virtual void RouteJsEvent(const std::string& name,
                            const JsArgList& args,
                            const JsEventHandler* target);

 private:
  // A struct used to hold the arguments to ProcessMessage() for
  // future invocation.
  struct PendingMessage {
    std::string name;
    JsArgList args;
    const JsEventHandler* sender;

    PendingMessage(const std::string& name, const JsArgList& args,
                   const JsEventHandler* sender);
  };

  typedef std::vector<PendingMessage> PendingMessageList;

  JsBackend* backend_;
  ObserverList<JsEventHandler> handlers_;
  PendingMessageList pending_messages_;

  DISALLOW_COPY_AND_ASSIGN(JsEventHandlerList);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_JS_EVENT_HANDLER_LIST_H_
