// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/sync_js_controller.h"

#include "base/location.h"
#include "chrome/browser/sync/js/js_backend.h"
#include "chrome/browser/sync/js/js_event_details.h"

namespace browser_sync {

SyncJsController::PendingJsMessage::PendingJsMessage(
    const std::string& name, const JsArgList& args,
    const WeakHandle<JsReplyHandler>& reply_handler)
    : name(name), args(args), reply_handler(reply_handler) {}

SyncJsController::PendingJsMessage::~PendingJsMessage() {}

SyncJsController::SyncJsController() {}

SyncJsController::~SyncJsController() {
  AttachJsBackend(WeakHandle<JsBackend>());
}

void SyncJsController::AddJsEventHandler(JsEventHandler* event_handler) {
  js_event_handlers_.AddObserver(event_handler);
  UpdateBackendEventHandler();
}

void SyncJsController::RemoveJsEventHandler(JsEventHandler* event_handler) {
  js_event_handlers_.RemoveObserver(event_handler);
  UpdateBackendEventHandler();
}

void SyncJsController::AttachJsBackend(
    const WeakHandle<JsBackend>& js_backend) {
  js_backend_ = js_backend;
  UpdateBackendEventHandler();

  if (js_backend_.IsInitialized()) {
    // Process any queued messages.
    for (PendingJsMessageList::const_iterator it =
             pending_js_messages_.begin();
         it != pending_js_messages_.end(); ++it) {
      js_backend_.Call(FROM_HERE, &JsBackend::ProcessJsMessage,
                       it->name, it->args, it->reply_handler);
    }
  }
}

void SyncJsController::ProcessJsMessage(
    const std::string& name, const JsArgList& args,
    const WeakHandle<JsReplyHandler>& reply_handler) {
  if (js_backend_.IsInitialized()) {
    js_backend_.Call(FROM_HERE, &JsBackend::ProcessJsMessage,
                     name, args, reply_handler);
  } else {
    pending_js_messages_.push_back(
        PendingJsMessage(name, args, reply_handler));
  }
}

void SyncJsController::HandleJsEvent(const std::string& name,
                                     const JsEventDetails& details) {
  FOR_EACH_OBSERVER(JsEventHandler, js_event_handlers_,
                    HandleJsEvent(name, details));
}

void SyncJsController::UpdateBackendEventHandler() {
  if (js_backend_.IsInitialized()) {
    // To avoid making the backend send useless events, we clear the
    // event handler we pass to it if we don't have any event
    // handlers.
    WeakHandle<JsEventHandler> backend_event_handler =
        (js_event_handlers_.size() > 0) ?
        MakeWeakHandle(AsWeakPtr()) : WeakHandle<SyncJsController>();
    js_backend_.Call(FROM_HERE, &JsBackend::SetJsEventHandler,
                     backend_event_handler);
  }
}

}  // namespace browser_sync
