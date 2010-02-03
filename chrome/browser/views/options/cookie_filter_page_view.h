// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_COOKIE_FILTER_PAGE_VIEW_H_
#define CHROME_BROWSER_VIEWS_OPTIONS_COOKIE_FILTER_PAGE_VIEW_H_

#include "chrome/browser/views/options/options_page_view.h"
#include "chrome/common/pref_member.h"
#include "views/controls/button/button.h"
#include "views/view.h"

namespace views {
class Checkbox;
class Label;
class NativeButton;
class RadioButton;
}

class PrefService;

////////////////////////////////////////////////////////////////////////////////
// CookieFilterPageView class is used to render the cookie content settings
// tab.

class CookieFilterPageView : public OptionsPageView,
                             public views::ButtonListener,
                             public views::LinkController {
 public:
  explicit CookieFilterPageView(Profile* profile);
  virtual ~CookieFilterPageView();

 private:
  // OptionsPageView implementation:
  virtual void InitControlLayout();

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from views::LinkController:
  virtual void LinkActivated(views::Link* source, int event_flags);

 private:
  // Controls for the cookie filter tab page view.
  views::RadioButton* allow_radio_;
  views::RadioButton* ask_radio_;
  views::RadioButton* block_radio_;
  views::NativeButton* exceptions_button_;
  views::Checkbox* block_3rdparty_check_;
  views::Checkbox* clear_on_close_check_;
  views::NativeButton* show_cookies_button_;

  DISALLOW_COPY_AND_ASSIGN(CookieFilterPageView);
};

#endif  // CHROME_BROWSER_VIEWS_OPTIONS_COOKIE_FILTER_PAGE_VIEW_H_

