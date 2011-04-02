// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_CONTAINER_VIEW_H_
#pragma once

#include "chrome/browser/ui/views/accessible_pane_view.h"
#include "chrome/browser/ui/views/infobars/infobar_container.h"

// The views-specific implementation of InfoBarContainer.
class InfoBarContainerView : public AccessiblePaneView,
                             public InfoBarContainer {
 public:
  explicit InfoBarContainerView(Delegate* delegate);
  virtual ~InfoBarContainerView();

  virtual int GetVerticalOverlap() OVERRIDE;

 private:
  // AccessiblePaneView:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // InfobarContainer:
  virtual void PlatformSpecificAddInfoBar(InfoBar* infobar) OVERRIDE;
  virtual void PlatformSpecificRemoveInfoBar(InfoBar* infobar) OVERRIDE;

  // Return the amount by which to overlap the toolbar above, and, when
  // |total_height| is non-NULL, set it to the height of the InfoBarContainer
  // (including overlap).
  int GetVerticalOverlap(int* total_height);

  DISALLOW_COPY_AND_ASSIGN(InfoBarContainerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_CONTAINER_VIEW_H_
