// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OUTDATED_UPGRADE_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OUTDATED_UPGRADE_BUBBLE_VIEW_H_

#include "base/macros.h"
#include "ui/views/bubble/bubble_dialog_delegate.h"

class ElevationIconSetter;

namespace content {
class PageNavigator;
}

// OutdatedUpgradeBubbleView warns the user that an upgrade is long overdue.
// It is intended to be used as the content of a bubble anchored off of the
// Chrome toolbar. Don't create an OutdatedUpgradeBubbleView directly,
// instead use the static ShowBubble method.
class OutdatedUpgradeBubbleView : public views::BubbleDialogDelegateView {
 public:
  static void ShowBubble(views::View* anchor_view,
                         content::PageNavigator* navigator,
                         bool auto_update_enabled);

  // Identifies if we are running a build that supports the
  // outdated upgrade bubble view.
  static bool IsAvailable();

  // views::BubbleDialogDelegateView methods.
  void WindowClosing() override;
  base::string16 GetWindowTitle() const override;
  bool Cancel() override;
  bool Accept() override;
  void UpdateButton(views::LabelButton* button, ui::DialogButton type) override;
  base::string16 GetDialogButtonLabel(ui::DialogButton button) const override;
  void Init() override;

 private:
  OutdatedUpgradeBubbleView(views::View* anchor_view,
                            content::PageNavigator* navigator,
                            bool auto_update_enabled);
  ~OutdatedUpgradeBubbleView() override;

  // Identifies if auto-update is enabled or not.
  bool auto_update_enabled_;

  // The PageNavigator to use for opening the Download Chrome URL.
  content::PageNavigator* navigator_;

  std::unique_ptr<ElevationIconSetter> elevation_icon_setter_;

  DISALLOW_COPY_AND_ASSIGN(OutdatedUpgradeBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OUTDATED_UPGRADE_BUBBLE_VIEW_H_
