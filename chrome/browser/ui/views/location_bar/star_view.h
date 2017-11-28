// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_VIEW_H_

#include "base/macros.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/views/location_bar/bubble_icon_view.h"

class Browser;
class CommandUpdater;
class WidgetObserver;

// The star icon to show a bookmark bubble.
class StarView : public BubbleIconView {
 public:
  StarView(CommandUpdater* command_updater, Browser* browser);
  ~StarView() override;

  // Toggles the star on or off.
  void SetToggled(bool on);

  // Shows the BookmarkPromoBubbleView when the BookmarkTracker calls for it.
  void ShowPromo();

 protected:
  // BubbleIconView:
  void OnExecuting(BubbleIconView::ExecuteSource execute_source) override;
  void OnWidgetDestroying(views::Widget* widget) override;
  void ExecuteCommand(ExecuteSource source) override;
  views::BubbleDialogDelegateView* GetBubble() const override;
  SkColor GetInkDropBaseColor() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  Browser* const browser_;

  // Observes the BookmarkPromoBubbleView's widget. Used to tell whether the
  // promo is open and gets called back when it closes.
  ScopedObserver<views::Widget, WidgetObserver> bookmark_promo_observer_;

  DISALLOW_COPY_AND_ASSIGN(StarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_VIEW_H_
