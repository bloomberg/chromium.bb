// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_MEMORY_INTERNALS_MEMORY_INTERNALS_HANDLER_H_
#define CHROME_BROWSER_UI_WEBUI_MEMORY_INTERNALS_MEMORY_INTERNALS_HANDLER_H_

#include <string>

#include "base/compiler_specific.h"
#include "base/memory/ref_counted.h"
#include "content/public/browser/web_ui_message_handler.h"

namespace base {
class ListValue;
}

class MemoryInternalsProxy;

// This class handles messages to and from MemoryInternalsUI.
// It does all its work on the IO thread through the proxy below.
class MemoryInternalsHandler : public content::WebUIMessageHandler {
 public:
  MemoryInternalsHandler();
  virtual ~MemoryInternalsHandler();

  virtual void RegisterMessages() OVERRIDE;

  // JavaScript message handlers.
  void OnJSUpdate(const base::ListValue* list);

  // MemoryInternals message handlers.
  void OnUpdate(const base::string16& update);

 private:
  scoped_refptr<MemoryInternalsProxy> proxy_;

  DISALLOW_COPY_AND_ASSIGN(MemoryInternalsHandler);
};

#endif  // CHROME_BROWSER_UI_WEBUI_MEMORY_INTERNALS_MEMORY_INTERNALS_HANDLER_H_
