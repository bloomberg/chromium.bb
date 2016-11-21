// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_EXTENSION_BINDINGS_SYSTEM_H_
#define EXTENSIONS_RENDERER_EXTENSION_BINDINGS_SYSTEM_H_

#include <string>

namespace base {
class DictionaryValue;
class ListValue;
}

namespace extensions {
class RequestSender;
class ScriptContext;

// The class responsible for creating extension bindings in different contexts,
// as well as dispatching requests and handling responses, and dispatching
// events to listeners.
// This is designed to be used on a single thread, but should be safe to use on
// threads other than the main thread (so that worker threads can have extension
// bindings).
class ExtensionBindingsSystem {
 public:
  virtual ~ExtensionBindingsSystem() {}

  // Called when a new ScriptContext is created.
  virtual void DidCreateScriptContext(ScriptContext* context) = 0;

  // Called when a ScriptContext is about to be released.
  virtual void WillReleaseScriptContext(ScriptContext* context) = 0;

  // Updates the bindings for a given |context|. This happens at initialization,
  // but also when e.g. an extension gets updated permissions.
  virtual void UpdateBindingsForContext(ScriptContext* context) = 0;

  // Dispatches an event with the given |name|, |event_args|, and
  // |filtering_info| in the given |context|.
  virtual void DispatchEventInContext(
      const std::string& event_name,
      const base::ListValue* event_args,
      const base::DictionaryValue* filtering_info,
      ScriptContext* context) = 0;

  // Handles the response associated with the given |request_id|.
  virtual void HandleResponse(int request_id,
                              bool success,
                              const base::ListValue& response,
                              const std::string& error) = 0;

  // Returns the associated RequestSender, if any.
  // TODO(devlin): Factor this out.
  virtual RequestSender* GetRequestSender() = 0;
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_EXTENSION_BINDINGS_SYSTEM_H_
