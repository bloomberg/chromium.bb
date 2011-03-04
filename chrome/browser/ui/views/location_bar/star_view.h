// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_VIEW_H_
#pragma once

#include "chrome/browser/ui/views/info_bubble.h"
#include "views/controls/image_view.h"

class CommandUpdater;
class InfoBubble;

namespace views {
class KeyEvent;
class MouseEvent;
}

class StarView : public views::ImageView, public InfoBubbleDelegate {
 public:
  explicit StarView(CommandUpdater* command_updater);
  virtual ~StarView();

  // Toggles the star on or off.
  void SetToggled(bool on);

 private:
  // views::ImageView overrides:
  virtual AccessibilityTypes::Role GetAccessibleRole();
  virtual bool GetTooltipText(const gfx::Point& p, std::wstring* tooltip);
  virtual bool OnMousePressed(const views::MouseEvent& event);
  virtual void OnMouseReleased(const views::MouseEvent& event, bool canceled);
  virtual bool OnKeyPressed(const views::KeyEvent& e);

  // InfoBubbleDelegate overrides:
  virtual void InfoBubbleClosing(InfoBubble* info_bubble,
                                 bool closed_by_escape);
  virtual bool CloseOnEscape();
  virtual bool FadeInOnShow();

  // The CommandUpdater for the Browser object that owns the location bar.
  CommandUpdater* command_updater_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(StarView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_LOCATION_BAR_STAR_VIEW_H_
