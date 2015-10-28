// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SAVE_CREDIT_CARD_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SAVE_CREDIT_CARD_ICON_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/location_bar/bubble_icon_view.h"

class Browser;
class CommandUpdater;

// The icon to show the Save Credit Card bubble where the user can choose to
// save the credit card info to use again later without re-entering it.
class SaveCreditCardIconView : public BubbleIconView {
 public:
  explicit SaveCreditCardIconView(CommandUpdater* command_updater,
                                  Browser* browser);
  ~SaveCreditCardIconView() override;

  // Toggles the icon on or off.
  void SetToggled(bool on);

 protected:
  // BubbleIconView:
  void OnExecuting(BubbleIconView::ExecuteSource execute_source) override;
  views::BubbleDelegateView* GetBubble() const override;
  gfx::VectorIconId GetVectorIcon() const override;

 private:
  // May be nullptr.
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(SaveCreditCardIconView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_SAVE_CREDIT_CARD_ICON_VIEW_H_
