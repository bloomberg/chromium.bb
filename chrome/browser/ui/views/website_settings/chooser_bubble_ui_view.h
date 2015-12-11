// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_CHOOSER_BUBBLE_UI_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_CHOOSER_BUBBLE_UI_VIEW_H_

#include "components/bubble/bubble_ui.h"
#include "ui/views/bubble/bubble_border.h"

namespace views {
class View;
}

class Browser;
class ChooserBubbleDelegate;
class ChooserBubbleUiViewDelegate;

// ChooserBubbleUiView implements a chooser-based permission model,
// it uses table view to show a list of items (such as usb devices, etc.)
// for user to grant permission. It can be used by WebUsb, WebBluetooth.
class ChooserBubbleUiView : public BubbleUi {
 public:
  ChooserBubbleUiView(Browser* browser,
                      ChooserBubbleDelegate* chooser_bubble_delegate);
  ~ChooserBubbleUiView() override;

  // BubbleUi:
  void Show(BubbleReference bubble_reference) override;
  void Close() override;
  void UpdateAnchorPosition() override;

 private:
  friend class ChooserBubbleUiViewDelegate;
  views::View* GetAnchorView();
  views::BubbleBorder::Arrow GetAnchorArrow();

  Browser* browser_;
  ChooserBubbleDelegate* chooser_bubble_delegate_;
  ChooserBubbleUiViewDelegate* chooser_bubble_ui_view_delegate_;
};

#endif  // CHROME_BROWSER_UI_VIEWS_WEBSITE_SETTINGS_CHOOSER_BUBBLE_UI_VIEW_H_
