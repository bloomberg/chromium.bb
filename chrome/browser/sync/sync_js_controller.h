// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_SYNC_SYNC_JS_CONTROLLER_H_
#define CHROME_BROWSER_SYNC_SYNC_JS_CONTROLLER_H_
#pragma once

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/sync/js/js_arg_list.h"
#include "chrome/browser/sync/js/js_controller.h"
#include "chrome/browser/sync/js/js_event_handler.h"
#include "chrome/browser/sync/util/weak_handle.h"

namespace browser_sync {

class JsBackend;

// A class that mediates between the sync JsEventHandlers and the sync
// JsBackend.
class SyncJsController
    : public JsController, public JsEventHandler,
      public base::SupportsWeakPtr<SyncJsController> {
 public:
  SyncJsController();

  virtual ~SyncJsController();

  // Sets the backend to route all messages to (if initialized).
  // Sends any queued-up messages if |backend| is initialized.
  void AttachJsBackend(const WeakHandle<JsBackend>& js_backend);

  // JsController implementation.
  virtual void AddJsEventHandler(JsEventHandler* event_handler) OVERRIDE;
  virtual void RemoveJsEventHandler(JsEventHandler* event_handler) OVERRIDE;
  // Queues up any messages that are sent when there is no attached
  // initialized backend.
  virtual void ProcessJsMessage(
      const std::string& name, const JsArgList& args,
      const WeakHandle<JsReplyHandler>& reply_handler) OVERRIDE;

  // JsEventHandler implementation.
  virtual void HandleJsEvent(const std::string& name,
                             const JsEventDetails& details) OVERRIDE;

 private:
  // A struct used to hold the arguments to ProcessJsMessage() for
  // future invocation.
  struct PendingJsMessage {
    std::string name;
    JsArgList args;
    WeakHandle<JsReplyHandler> reply_handler;

    PendingJsMessage(const std::string& name, const JsArgList& args,
                     const WeakHandle<JsReplyHandler>& reply_handler);

    ~PendingJsMessage();
  };

  typedef std::vector<PendingJsMessage> PendingJsMessageList;

  // Sets |js_backend_|'s event handler depending on how many
  // underlying event handlers we have.
  void UpdateBackendEventHandler();

  WeakHandle<JsBackend> js_backend_;
  ObserverList<JsEventHandler> js_event_handlers_;
  PendingJsMessageList pending_js_messages_;

  DISALLOW_COPY_AND_ASSIGN(SyncJsController);
};

}  // namespace browser_sync

#endif  // CHROME_BROWSER_SYNC_SYNC_JS_CONTROLLER_H_
