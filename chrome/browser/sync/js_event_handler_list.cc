// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/sync/js_event_handler_list.h"

#include <cstddef>

#include "base/logging.h"
#include "chrome/browser/sync/js_backend.h"
#include "chrome/browser/sync/js_event_handler.h"

namespace browser_sync {

JsEventHandlerList::PendingMessage::PendingMessage(
    const std::string& name, const JsArgList& args,
    const JsEventHandler* sender)
    : name(name), args(args), sender(sender) {}

JsEventHandlerList::JsEventHandlerList() : backend_(NULL) {}

JsEventHandlerList::~JsEventHandlerList() {
  RemoveBackend();
}

// We connect to the backend only when necessary, i.e. when there is
// at least one handler.

void JsEventHandlerList::AddHandler(JsEventHandler* handler) {
  handlers_.AddObserver(handler);
  if (backend_) {
    backend_->SetParentJsEventRouter(this);
  }
}

void JsEventHandlerList::RemoveHandler(JsEventHandler* handler) {
  handlers_.RemoveObserver(handler);
  if (backend_ && handlers_.size() == 0) {
    backend_->RemoveParentJsEventRouter();
  }
}

void JsEventHandlerList::SetBackend(JsBackend* backend) {
  DCHECK(!backend_);
  DCHECK(backend);
  backend_ = backend;

  if (handlers_.size() > 0) {
    backend_->SetParentJsEventRouter(this);

    // Process any queued messages.
    PendingMessageList pending_messages;
    pending_messages_.swap(pending_messages);
    for (PendingMessageList::const_iterator it = pending_messages.begin();
         it != pending_messages.end(); ++it) {
      backend_->ProcessMessage(it->name, it->args, it->sender);
    }
  }
}

void JsEventHandlerList::RemoveBackend() {
  if (backend_) {
    backend_->RemoveParentJsEventRouter();
    backend_ = NULL;
  }
}

void JsEventHandlerList::ProcessMessage(
    const std::string& name, const JsArgList& args,
    const JsEventHandler* sender) {
  if (backend_) {
    backend_->ProcessMessage(name, args, sender);
  } else {
    pending_messages_.push_back(PendingMessage(name, args, sender));
  }
}

void JsEventHandlerList::RouteJsEvent(const std::string& name,
                                      const JsArgList& args,
                                      const JsEventHandler* target) {
  if (target) {
    JsEventHandler* non_const_target(const_cast<JsEventHandler*>(target));
    if (handlers_.HasObserver(non_const_target)) {
      non_const_target->HandleJsEvent(name, args);
    } else {
      VLOG(1) << "Unknown target; dropping event " << name
              << " with args " << args.ToString();
    }
  } else {
    FOR_EACH_OBSERVER(JsEventHandler, handlers_, HandleJsEvent(name, args));
  }
}

}  // namespace browser_sync
