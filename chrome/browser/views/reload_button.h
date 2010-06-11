// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_RELOAD_BUTTON_H__
#define CHROME_BROWSER_VIEWS_RELOAD_BUTTON_H__

#include "base/basictypes.h"
#include "base/timer.h"
#include "views/controls/button/image_button.h"

class Browser;
class LocationBarView;

////////////////////////////////////////////////////////////////////////////////
//
// ReloadButton
//
// The reload button in the toolbar, which changes to a stop button when a page
// load is in progress. Trickiness comes from the desire to have the 'stop'
// button not change back to 'reload' if the user's mouse is hovering over it
// (to prevent mis-clicks).
//
////////////////////////////////////////////////////////////////////////////////

class ReloadButton : public views::ToggleImageButton,
                     public views::ButtonListener {
 public:
  enum Mode { MODE_RELOAD = 0, MODE_STOP };

  ReloadButton(LocationBarView* location_bar, Browser* Browser);
  virtual ~ReloadButton();

  // Ask for a specified button state.  If |force| is true this will be applied
  // immediately.
  void ChangeMode(Mode mode, bool force);

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* button, const views::Event& event);

  // Overridden from views::View:
  virtual void OnMouseExited(const views::MouseEvent& e);
  virtual bool GetTooltipText(const gfx::Point& p, std::wstring* tooltip);

 private:
  void OnButtonTimer();

  base::OneShotTimer<ReloadButton> timer_;

  LocationBarView* location_bar_;
  Browser* browser_;

  // The mode we should be in
  Mode intended_mode_;

  // The currently-visible mode - this may different from the intended mode
  Mode visible_mode_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ReloadButton);
};

#endif  // CHROME_BROWSER_VIEWS_RELOAD_BUTTON_H__
