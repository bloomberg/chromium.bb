// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_CONTAINER_VIEW_H_

#include "chrome/browser/infobars/infobar_container.h"
#include "ui/views/accessible_pane_view.h"

// The views-specific implementation of InfoBarContainer.
class InfoBarContainerView : public views::AccessiblePaneView,
                             public InfoBarContainer {
 public:
  static const char kViewClassName[];

  explicit InfoBarContainerView(Delegate* delegate,
                                SearchModel* search_model);
  virtual ~InfoBarContainerView();

 private:
  // AccessiblePaneView:
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual std::string GetClassName() const OVERRIDE;
  virtual void Layout() OVERRIDE;
  virtual void GetAccessibleState(ui::AccessibleViewState* state) OVERRIDE;

  // InfobarContainer:
  virtual void PlatformSpecificAddInfoBar(InfoBar* infobar,
                                          size_t position) OVERRIDE;
  virtual void PlatformSpecificRemoveInfoBar(InfoBar* infobar) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(InfoBarContainerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_CONTAINER_VIEW_H_
