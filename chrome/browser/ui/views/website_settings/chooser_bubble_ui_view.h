// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_CHOOSER_BUBBLE_UI_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_CHOOSER_BUBBLE_UI_VIEW_H_

#include <memory>

#include "base/macros.h"
#include "components/bubble/bubble_ui.h"
#include "ui/views/bubble/bubble_border.h"

namespace views {
class View;
}

class Browser;
class ChooserController;
class ChooserBubbleUiViewDelegate;

// ChooserBubbleUiView implements a chooser-based permission model,
// it uses table view to show a list of items (such as usb devices, etc.)
// for user to grant permission. It can be used by the WebUSB or WebBluetooth
// APIs. It is owned by the BubbleController, which is owned by the
// BubbleManager.
class ChooserBubbleUiView : public BubbleUi {
 public:
  ChooserBubbleUiView(Browser* browser,
                      std::unique_ptr<ChooserController> chooser_controller);
  ~ChooserBubbleUiView() override;

  // BubbleUi:
  void Show(BubbleReference bubble_reference) override;
  void Close() override;
  void UpdateAnchorPosition() override;

 private:
  views::View* GetAnchorView();
  views::BubbleBorder::Arrow GetAnchorArrow();

  Browser* browser_;  // Weak.
  // Weak. Owned by its parent view.
  ChooserBubbleUiViewDelegate* chooser_bubble_ui_view_delegate_;

  DISALLOW_COPY_AND_ASSIGN(ChooserBubbleUiView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_CHOOSER_BUBBLE_UI_VIEW_H_
