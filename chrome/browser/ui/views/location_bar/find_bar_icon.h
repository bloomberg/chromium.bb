// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_FIND_BAR_ICON_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_FIND_BAR_ICON_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/location_bar/bubble_icon_view.h"

// The find icon to show when the find bar is visible.
class FindBarIcon : public BubbleIconView {
 public:
  FindBarIcon();
  ~FindBarIcon() override;

 protected:
  // BubbleIconView:
  void OnExecuting(ExecuteSource execute_source) override;
  views::BubbleDialogDelegateView* GetBubble() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(FindBarIcon);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_FIND_BAR_ICON_H_
