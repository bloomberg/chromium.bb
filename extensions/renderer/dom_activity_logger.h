// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_RENDERER_DOM_ACTIVITY_LOGGER_H_
#define EXTENSIONS_RENDERER_DOM_ACTIVITY_LOGGER_H_

#include <string>

#include "base/macros.h"
#include "third_party/WebKit/public/web/WebDOMActivityLogger.h"

namespace content {
class V8ValueConverter;
}

namespace extensions {

class ActivityLogConverterStrategy;

// Used to log DOM API calls from within WebKit. The events are sent via IPC to
// extensions::ActivityLog for recording and display.
class DOMActivityLogger: public blink::WebDOMActivityLogger {
 public:
  static const int kMainWorldId = 0;
  explicit DOMActivityLogger(const std::string& extension_id);
  virtual ~DOMActivityLogger();

  // Marshalls the arguments into an ExtensionHostMsg_DOMAction_Params
  // and sends it over to the browser (via IPC) for appending it to the
  // extension activity log.
  // (Overrides the log method in blink::WebDOMActivityLogger)
  virtual void log(const blink::WebString& api_name,
                   int argc,
                   const v8::Handle<v8::Value> argv[],
                   const blink::WebString& call_type,
                   const blink::WebURL& url,
                   const blink::WebString& title) OVERRIDE;

  // Check (using the WebKit API) if there is no logger attached to the world
  // corresponding to world_id, and if so, construct a new logger and attach it.
  // world_id = 0 indicates the main world.
  static void AttachToWorld(int world_id,
                            const std::string& extension_id);

 private:
  std::string extension_id_;

  DISALLOW_COPY_AND_ASSIGN(DOMActivityLogger);
};

}  // namespace extensions

#endif  // EXTENSIONS_RENDERER_DOM_ACTIVITY_LOGGER_H_

