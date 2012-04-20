// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EXTENSION_REQUEST_SENDER_H_
#define CHROME_RENDERER_EXTENSIONS_EXTENSION_REQUEST_SENDER_H_
#pragma once

#include <string>
#include <map>

#include "base/memory/linked_ptr.h"
#include "v8/include/v8.h"

class ChromeV8ContextSet;
class ExtensionDispatcher;

namespace base {
class ListValue;
}

struct PendingRequest;

// Responsible for sending requests for named extension API functions to the
// extension host and routing the responses back to the caller.
class ExtensionRequestSender {
 public:
  explicit ExtensionRequestSender(ExtensionDispatcher* extension_dispatcher,
                                  ChromeV8ContextSet* context_set);
  ~ExtensionRequestSender();

  // Makes a call to the API function |name| that is to be handled by the
  // extension host. The response to this request will be received in
  // HandleResponse().
  // TODO(koz): Remove |request_id| and generate that internally.
  void StartRequest(const std::string& name,
                    int request_id,
                    bool has_callback,
                    bool for_io_thread,
                    base::ListValue* value_args);

  // Handles responses from the extension host to calls made by StartRequest().
  void HandleResponse(int request_id,
                      bool success,
                      const base::ListValue& response,
                      const std::string& error);


 private:
  typedef std::map<int, linked_ptr<PendingRequest> > PendingRequestMap;

  void InsertRequest(int request_id, PendingRequest* pending_request);
  linked_ptr<PendingRequest> RemoveRequest(int request_id);

  ExtensionDispatcher* extension_dispatcher_;
  PendingRequestMap pending_requests_;
  ChromeV8ContextSet* context_set_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionRequestSender);
};

#endif  // CHROME_RENDERER_EXTENSIONS_EXTENSION_REQUEST_SENDER_H_
