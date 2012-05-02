// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_SESSIONS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_SESSIONS_UI_H_
#pragma once

#include "content/public/browser/web_ui_controller.h"

namespace base {
class RefCountedMemory;
}

class SessionsUI : public content::WebUIController {
 public:
  explicit SessionsUI(content::WebUI* web_ui);

  static base::RefCountedMemory* GetFaviconResourceBytes();

 private:
  DISALLOW_COPY_AND_ASSIGN(SessionsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_SESSIONS_UI_H_
