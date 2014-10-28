// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_COMMAND_HANDLER_X11_H_
#define CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_COMMAND_HANDLER_X11_H_

#include "ui/events/event_handler.h"

class Browser;

class BrowserCommandHandlerX11 : public ui::EventHandler {
 public:
  explicit BrowserCommandHandlerX11(Browser* browser);
  ~BrowserCommandHandlerX11() override;

 private:
  // ui::EventHandler:
  void OnMouseEvent(ui::MouseEvent* event) override;

  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(BrowserCommandHandlerX11);
};

#endif  // CHROME_BROWSER_UI_VIEWS_FRAME_BROWSER_COMMAND_HANDLER_X11_H_
