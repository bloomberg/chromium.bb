// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_INFOBARS_MEDIA_STREAM_INFOBAR_H_
#define CHROME_BROWSER_UI_VIEWS_INFOBARS_MEDIA_STREAM_INFOBAR_H_
#pragma once

#include "chrome/browser/infobars/infobar_delegate.h"
#include "chrome/browser/media/media_stream_devices_menu_model.h"
#include "chrome/browser/ui/views/infobars/infobar_background.h"
#include "chrome/browser/ui/views/infobars/infobar_view.h"
#include "ui/views/controls/button/menu_button_listener.h"

class MediaStreamInfoBarDelegate;

namespace views {
class MenuButton;
}

// Views implementation of the Media Stream InfoBar, which asks the user whether
// he wants to grant access to his camera/microphone to the current webpage. It
// also provides a way for the user to select which device(s) will be used by
// the page.
class MediaStreamInfoBar : public InfoBarView,
                           public views::MenuButtonListener {
 public:
  MediaStreamInfoBar(InfoBarTabHelper* contents,
                     MediaStreamInfoBarDelegate* delegate);

 private:
  virtual ~MediaStreamInfoBar();

  // InfoBarView:
  virtual void Layout() OVERRIDE;
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child) OVERRIDE;
  virtual void ButtonPressed(views::Button* sender,
                             const views::Event& event) OVERRIDE;
  virtual int ContentMinimumWidth() const OVERRIDE;

  // views::MenuButtonListener:
  virtual void OnMenuButtonClicked(views::View* source,
                                   const gfx::Point& point) OVERRIDE;

  MediaStreamInfoBarDelegate* GetDelegate();

  views::Label* label_;
  views::TextButton* allow_button_;
  views::TextButton* deny_button_;
  views::MenuButton* devices_menu_button_;

  MediaStreamDevicesMenuModel devices_menu_model_;

  DISALLOW_COPY_AND_ASSIGN(MediaStreamInfoBar);
};

#endif  // CHROME_BROWSER_UI_VIEWS_INFOBARS_MEDIA_STREAM_INFOBAR_H_
