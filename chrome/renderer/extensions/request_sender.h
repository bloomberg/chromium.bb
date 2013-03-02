// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_REQUEST_SENDER_H_
#define CHROME_RENDERER_EXTENSIONS_REQUEST_SENDER_H_

#include <string>
#include <map>

#include "base/memory/linked_ptr.h"
#include "v8/include/v8.h"

namespace base {
class ListValue;
}

namespace extensions {
class ChromeV8Context;
class Dispatcher;

struct PendingRequest;

// Responsible for sending requests for named extension API functions to the
// extension host and routing the responses back to the caller.
class RequestSender {
 public:
  explicit RequestSender(Dispatcher* dispatcher);
  ~RequestSender();

  // Makes a call to the API function |name| that is to be handled by the
  // extension host. The response to this request will be received in
  // HandleResponse().
  // TODO(koz): Remove |request_id| and generate that internally.
  //            There are multiple of these per render view though, so we'll
  //            need to vend the IDs centrally.
  void StartRequest(ChromeV8Context* target_context,
                    const std::string& name,
                    int request_id,
                    bool has_callback,
                    bool for_io_thread,
                    base::ListValue* value_args);

  // Handles responses from the extension host to calls made by StartRequest().
  void HandleResponse(int request_id,
                      bool success,
                      const base::ListValue& response,
                      const std::string& error);

  // Notifies this that a context is no longer valid.
  // TODO(kalman): Do this in a generic/safe way.
  void InvalidateContext(ChromeV8Context* context);

 private:
  typedef std::map<int, linked_ptr<PendingRequest> > PendingRequestMap;

  void InsertRequest(int request_id, PendingRequest* pending_request);
  linked_ptr<PendingRequest> RemoveRequest(int request_id);

  Dispatcher* dispatcher_;
  PendingRequestMap pending_requests_;

  DISALLOW_COPY_AND_ASSIGN(RequestSender);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_REQUEST_SENDER_H_
