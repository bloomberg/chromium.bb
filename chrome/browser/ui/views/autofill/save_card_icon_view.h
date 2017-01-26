// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AUTOFILL_SAVE_CARD_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AUTOFILL_SAVE_CARD_ICON_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/views/location_bar/bubble_icon_view.h"

class Browser;
class CommandUpdater;

namespace autofill {

class SaveCardBubbleControllerImpl;

// The location bar icon to show the Save Credit Card bubble where the user can
// choose to save the credit card info to use again later without re-entering
// it.
class SaveCardIconView : public BubbleIconView, public TabStripModelObserver {
 public:
  explicit SaveCardIconView(CommandUpdater* command_updater, Browser* browser);
  ~SaveCardIconView() override;

 protected:
  // BubbleIconView:
  void OnExecuting(BubbleIconView::ExecuteSource execute_source) override;
  views::BubbleDialogDelegateView* GetBubble() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;

  // TabStripModelObserver:
  void TabDeactivated(content::WebContents* contents) override;

 private:
  SaveCardBubbleControllerImpl* GetController() const;

  // May be nullptr.
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(SaveCardIconView);
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_VIEWS_AUTOFILL_SAVE_CARD_ICON_VIEW_H_
