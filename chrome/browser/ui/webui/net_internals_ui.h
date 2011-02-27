// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_UI_H_
#define CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_UI_H_
#pragma once

#include "content/browser/webui/web_ui.h"

class NetInternalsUI : public WebUI {
 public:
  explicit NetInternalsUI(TabContents* contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(NetInternalsUI);
};

#endif  // CHROME_BROWSER_UI_WEBUI_NET_INTERNALS_UI_H_
