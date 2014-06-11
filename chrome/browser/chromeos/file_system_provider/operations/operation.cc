// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/file_system_provider/operations/operation.h"

#include "chrome/browser/chromeos/file_system_provider/provided_file_system_info.h"
#include "extensions/browser/event_router.h"

namespace chromeos {
namespace file_system_provider {
namespace operations {
namespace {

// Default implementation for dispatching an event. Can be replaced for unit
// tests by Operation::SetDispatchEventImplForTest().
bool DispatchEventImpl(extensions::EventRouter* event_router,
                       const std::string& extension_id,
                       scoped_ptr<extensions::Event> event) {
  if (!event_router->ExtensionHasEventListener(extension_id, event->event_name))
    return false;

  event_router->DispatchEventToExtension(extension_id, event.Pass());
  return true;
}

}  // namespace

Operation::Operation(extensions::EventRouter* event_router,
                     const ProvidedFileSystemInfo& file_system_info)
    : file_system_info_(file_system_info),
      dispatch_event_impl_(base::Bind(&DispatchEventImpl,
                                      event_router,
                                      file_system_info_.extension_id())) {
}

Operation::~Operation() {
}

void Operation::SetDispatchEventImplForTesting(
    const DispatchEventImplCallback& callback) {
  dispatch_event_impl_ = callback;
}

bool Operation::SendEvent(int request_id,
                          const std::string& event_name,
                          scoped_ptr<base::DictionaryValue> options) {
  options->SetString("fileSystemId", file_system_info_.file_system_id());
  options->SetInteger("requestId", request_id);

  scoped_ptr<base::ListValue> event_args(new base::ListValue);
  event_args->Append(options.release());

  return dispatch_event_impl_.Run(
      make_scoped_ptr(new extensions::Event(event_name, event_args.Pass())));
}

}  // namespace operations
}  // namespace file_system_provider
}  // namespace chromeos
