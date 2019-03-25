// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_HEADER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_HEADER_H_

#include "base/strings/string16.h"
#include "ui/views/view.h"

namespace gfx {
class Canvas;
}

// View for tab group headers in the tab strip, which are tab-shaped markers of
// group boundaries. There is one header for each group, which is included in
// the tab strip flow and positioned left of the leftmost tab in the group.
class TabGroupHeader : public views::View {
 public:
  explicit TabGroupHeader(const base::string16& group_title);

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabGroupHeader);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_TAB_GROUP_HEADER_H_
