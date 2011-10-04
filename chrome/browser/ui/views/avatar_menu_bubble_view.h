// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_AVATAR_MENU_BUBBLE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_AVATAR_MENU_BUBBLE_VIEW_H_
#pragma once

#include <vector>

#include "chrome/browser/profiles/avatar_menu_model_observer.h"
#include "chrome/browser/ui/views/bubble/bubble.h"
#include "views/controls/button/custom_button.h"
#include "views/controls/link.h"
#include "views/controls/link_listener.h"
#include "views/controls/separator.h"

class AvatarMenuModel;
class Browser;

// This bubble view is displayed when the user clicks on the avatar button.
// It displays a list of profiles and allows users to switch between profiles.
class AvatarMenuBubbleView : public views::View,
                             public views::ButtonListener,
                             public views::LinkListener,
                             public BubbleDelegate,
                             public AvatarMenuModelObserver {
 public:
  explicit AvatarMenuBubbleView(Browser* browser);
  virtual ~AvatarMenuBubbleView();

  // views::View implementation.
  virtual gfx::Size GetPreferredSize() OVERRIDE;
  virtual void Layout() OVERRIDE;

  // views::ButtonListener implementation.
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;

  // views::LinkListener implementation.
  virtual void LinkClicked(views::Link* source, int event_flags) OVERRIDE;

  // BubbleDelegate implementation.
  virtual void BubbleClosing(Bubble* bubble, bool closed_by_escape) OVERRIDE;
  virtual bool CloseOnEscape() OVERRIDE;
  virtual bool FadeInOnShow() OVERRIDE;

  // AvatarMenuModelObserver implementation.
  virtual void OnAvatarMenuModelChanged(
      AvatarMenuModel* avatar_menu_model) OVERRIDE;

 private:
  views::Link* add_profile_link_;
  scoped_ptr<AvatarMenuModel> avatar_menu_model_;
  Browser* browser_;
  std::vector<views::CustomButton*> item_views_;
  views::Separator* separator_;

  DISALLOW_COPY_AND_ASSIGN(AvatarMenuBubbleView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_AVATAR_MENU_BUBBLE_VIEW_H_
