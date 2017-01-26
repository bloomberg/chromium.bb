// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_ICON_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_ICON_VIEW_H_

#include "base/macros.h"
#include "chrome/browser/ui/views/location_bar/bubble_icon_view.h"

class CommandUpdater;

// The location bar icon to show the Translate bubble where the user can have
// the page translated.
class TranslateIconView : public BubbleIconView {
 public:
  explicit TranslateIconView(CommandUpdater* command_updater);
  ~TranslateIconView() override;

 protected:
  // BubbleIconView:
  void OnExecuting(BubbleIconView::ExecuteSource execute_source) override;
  void OnPressed(bool activated) override;
  views::BubbleDialogDelegateView* GetBubble() const override;
  const gfx::VectorIcon& GetVectorIcon() const override;

 private:
  DISALLOW_COPY_AND_ASSIGN(TranslateIconView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TRANSLATE_TRANSLATE_ICON_VIEW_H_
