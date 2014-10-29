// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_CONTAINER_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_CONTAINER_VIEW_H_

#include "components/infobars/core/infobar_container.h"
#include "ui/views/accessible_pane_view.h"

// The views-specific implementation of InfoBarContainer.
class InfoBarContainerView : public views::AccessiblePaneView,
                             public infobars::InfoBarContainer {
 public:
  static const char kViewClassName[];

  explicit InfoBarContainerView(Delegate* delegate);
  ~InfoBarContainerView() override;

 private:
  // AccessiblePaneView:
  gfx::Size GetPreferredSize() const override;
  const char* GetClassName() const override;
  void Layout() override;
  void GetAccessibleState(ui::AXViewState* state) override;

  // InfobarContainer:
  void PlatformSpecificAddInfoBar(infobars::InfoBar* infobar,
                                  size_t position) override;
  void PlatformSpecificRemoveInfoBar(infobars::InfoBar* infobar) override;

  DISALLOW_COPY_AND_ASSIGN(InfoBarContainerView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_INFOBAR_CONTAINER_VIEW_H_
