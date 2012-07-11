// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_NET_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_NET_INTERNALS_UI_H_
#include "content/public/browser/web_ui_controller.h"

namespace base {
class Value;
}

class NetInternalsUI : public content::WebUIController {
 public:
  explicit NetInternalsUI(content::WebUI* web_ui);

  // Returns a Value containing constants NetInternals needs to load a log file.
  // Safe to call on any thread.  Caller takes ownership of the returned Value.
  static base::Value* GetConstants();

 private:
  DISALLOW_COPY_AND_ASSIGN(NetInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_NET_INTERNALS_UI_H_
