// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_BUBBLE_BORDER_CONTENTS_H_
#define CHROME_BROWSER_UI_VIEWS_BUBBLE_BORDER_CONTENTS_H_
#pragma once

#include "ui/views/bubble/border_contents_view.h"

// This is used to paint the border of the Bubble. Windows uses this via
// BorderWidgetWin, while others can use it directly in the bubble.
class BorderContents : public views::BorderContentsView {
 public:
  BorderContents();

 protected:
  virtual ~BorderContents() { }

  // views::BorderContentsView overrides:
  virtual gfx::Rect GetMonitorBounds(const gfx::Rect& rect) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(BorderContents);
};

#endif  // CHROME_BROWSER_UI_VIEWS_BUBBLE_BORDER_CONTENTS_H_
