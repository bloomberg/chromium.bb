// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_WEBUI_SLIDESHOW_UI_H_
#define CHROME_BROWSER_WEBUI_SLIDESHOW_UI_H_
#pragma once

#include "content/browser/webui/web_ui.h"

class SlideshowUI : public WebUI {
 public:
  explicit SlideshowUI(TabContents* contents);

 private:
  DISALLOW_COPY_AND_ASSIGN(SlideshowUI);
};

#endif  // CHROME_BROWSER_WEBUI_SLIDESHOW_UI_H_
