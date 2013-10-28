// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_VIEW_H_

#include "chrome/browser/ui/views/location_bar/bubble_icon_view.h"

class CommandUpdater;

// The star icon to show a bookmark bubble.
class StarView : public BubbleIconView {
 public:
  explicit StarView(CommandUpdater* command_updater);
  virtual ~StarView();

  // Toggles the star on or off.
  void SetToggled(bool on);

 protected:
  // BubbleIconView:
  virtual bool IsBubbleShowing() const OVERRIDE;
  virtual void OnExecuting(
      BubbleIconView::ExecuteSource execute_source) OVERRIDE;

 private:
  DISALLOW_COPY_AND_ASSIGN(StarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_VIEW_H_
