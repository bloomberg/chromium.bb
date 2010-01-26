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

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from views::LinkController:
  virtual void LinkActivated(views::Link* source, int event_flags);

 private:
  typedef enum {
    COOKIES_ALLOWED_DEFAULT = 0,
    COOKIES_ALLOWED_ALLOW = 0,
    COOKIES_ALLOWED_ASK,
    COOKIES_ALLOWED_BLOCK,
  } CookiesAllowed;

  // OptionsPageView implementation:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::wstring* pref_name) {}

  // Handles that allowed radio button state has changed.
  void SetAllowCookies(CookiesAllowed allowed);

  // Handles click on exceptions dialog.
  void ShowCookieExceptionsDialog();

  // Handles change in block 3rd party cookies checkbox.
  void SetBlock3rdPartyCookies(bool block);

  // Handles change in clean on close checkbox.
  void SetClearCookiesOnClose(bool clear);

  // Handles click on flash settings link.
  void OpenFlashSettingsDialog();

  // Opens cookie manager dialog.
  void ShowCookieManagerDialog();

 private:
  // Controls for the cookie filter tab page view.
  views::Label* caption_label_;
  views::RadioButton* allow_radio_;
  views::RadioButton* ask_radio_;
  views::RadioButton* block_radio_;
  views::NativeButton* exceptions_button_;
  views::Checkbox* block_3rdparty_check_;
  views::Checkbox* clear_on_close_check_;
  views::NativeButton* show_cookies_button_;
  views::Link* flash_settings_link_;

  DISALLOW_COPY_AND_ASSIGN(CookieFilterPageView);
};

#endif  // CHROME_BROWSER_VIEWS_OPTIONS_COOKIE_FILTER_PAGE_VIEW_H_

