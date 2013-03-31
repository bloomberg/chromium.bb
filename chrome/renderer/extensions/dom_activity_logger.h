// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_DOM_ACTIVITY_LOGGER_H_
#define CHROME_RENDERER_EXTENSIONS_DOM_ACTIVITY_LOGGER_H_

#include <string>

#include "base/strings/string_piece.h"
#include "googleurl/src/gurl.h"
#include "third_party/WebKit/Source/Platform/chromium/public/WebString.h"
#include "third_party/WebKit/Source/WebKit/chromium/public/WebDOMActivityLogger.h"
#include "v8/include/v8.h"

using WebKit::WebString;

// Used to log DOM API calls from within WebKit. The events are sent via IPC to
// extensions::ActivityLog for recording and display.
namespace extensions {

class DOMActivityLogger: public WebKit::WebDOMActivityLogger {
 public:
  static const int kMainWorldId = 0;
  DOMActivityLogger(const std::string& extension_id,
                    const GURL& url,
                    const string16& title);

  // Marshalls the arguments into an ExtensionHostMsg_DOMAction_Params
  // and sends it over to the browser (via IPC) for appending it to the
  // extension activity log.
  // (Overrides the log method in WebKit::WebDOMActivityLogger)
  virtual void log(const WebString& api_name,
                   int argc,
                   const v8::Handle<v8::Value> argv[],
                   const WebString& extra_info);

  // If extension activity logging is enabled then check (using the
  // WebKit API) if there is no logger attached to the world corresponding
  // to world_id, and if so, construct a new logger and attach it.
  // worl_id = 0 indicates the main world.
  static void AttachToWorld(int world_id,
                            const std::string& extension_id,
                            const GURL& url,
                            const string16& title);

 private:
  std::string extension_id_;
  GURL url_;
  string16 title_;

  DISALLOW_COPY_AND_ASSIGN(DOMActivityLogger);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_DOM_ACTIVITY_LOGGER_H_

