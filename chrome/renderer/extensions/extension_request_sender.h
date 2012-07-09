// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EXTENSION_REQUEST_SENDER_H_
#define CHROME_RENDERER_EXTENSIONS_EXTENSION_REQUEST_SENDER_H_
#pragma once

#include <string>
#include <map>

#include "base/compiler_specific.h"
#include "base/memory/linked_ptr.h"
#include "base/scoped_observer.h"
#include "content/public/renderer/render_process_observer.h"
#include "v8/include/v8.h"

class ChromeV8ContextSet;
class ExtensionDispatcher;

namespace base {
class ListValue;
}

namespace content {
class RenderThread;
}

struct PendingRequest;

// Responsible for sending requests for named extension API functions to the
// extension host and routing the responses back to the caller.
class ExtensionRequestSender : content::RenderProcessObserver {
 public:
  explicit ExtensionRequestSender(ExtensionDispatcher* extension_dispatcher,
                                  ChromeV8ContextSet* context_set);
  virtual ~ExtensionRequestSender();

  // Makes a call to the API function |name| that is to be handled by the
  // extension host.
  // Returns the request ID of the request, or -1 if no request was made.
  int StartRequest(const std::string& name,
                   bool has_callback,
                   bool for_io_thread,
                   base::ListValue* value_args);

 private:
  // content::RenderProcessObserver implementation.
  virtual bool OnControlMessageReceived(const IPC::Message& message) OVERRIDE;

  void OnExtensionResponse(int request_id,
                           bool success,
                           const base::ListValue& response,
                           const std::string& error);

  void InsertRequest(int request_id, PendingRequest* pending_request);
  linked_ptr<PendingRequest> RemoveRequest(int request_id);

  // Owner (weak reference).
  ExtensionDispatcher* extension_dispatcher_;

  // V8 contexts (weak reference).
  ChromeV8ContextSet* context_set_;

  // Scoped observer to RenderProcessHost.
  ScopedObserver<content::RenderThread, content::RenderProcessObserver>
      scoped_observer_;

  // The next request ID.
  int next_request_id_;

  // Pending requests.
  typedef std::map<int, linked_ptr<PendingRequest> > PendingRequestMap;
  PendingRequestMap pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionRequestSender);
};

#endif  // CHROME_RENDERER_EXTENSIONS_EXTENSION_REQUEST_SENDER_H_
