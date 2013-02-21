// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_HOME_BUTTON_H_
#define CHROME_BROWSER_UI_VIEWS_HOME_BUTTON_H_

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "ui/views/controls/button/image_button.h"

class Browser;

class HomeImageButton : public views::ImageButton {
 public:
  HomeImageButton(views::ButtonListener* listener, Browser* browser);
  virtual ~HomeImageButton();

  // views::ImageButton:
  virtual bool GetDropFormats(
      int* formats,
      std::set<OSExchangeData::CustomFormat>* custom_formats) OVERRIDE;
  virtual bool CanDrop(const OSExchangeData& data) OVERRIDE;
  virtual int OnDragUpdated(const ui::DropTargetEvent& event) OVERRIDE;
  virtual int OnPerformDrop(const ui::DropTargetEvent& event) OVERRIDE;

 private:
  Browser* browser_;

  DISALLOW_COPY_AND_ASSIGN(HomeImageButton);
};

#endif  // CHROME_BROWSER_UI_VIEWS_HOME_BUTTON_H_
