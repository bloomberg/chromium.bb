// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OPTIONS_COOKIE_FILTER_PAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OPTIONS_COOKIE_FILTER_PAGE_VIEW_H_
#pragma once

#include "chrome/browser/ui/views/options/content_filter_page_view.h"

#include "chrome/browser/prefs/pref_member.h"

namespace views {
class Checkbox;
}

////////////////////////////////////////////////////////////////////////////////
// CookieFilterPageView class is used to render the cookie content settings tab.

class CookieFilterPageView : public ContentFilterPageView,
                             public views::LinkController {
 public:
  explicit CookieFilterPageView(Profile* profile);
  virtual ~CookieFilterPageView();

 private:
  // Overridden from ContentFilterPageView:
  virtual void InitControlLayout();

  // OptionsPageView implementation:
  virtual void NotifyPrefChanged(const std::string* pref_name);

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from views::LinkController:
  virtual void LinkActivated(views::Link* source, int event_flags);

 private:
  // Controls for the cookie filter tab page view.
  views::Checkbox* block_3rdparty_check_;
  views::Checkbox* clear_on_close_check_;
  views::NativeButton* show_cookies_button_;

  // Clear locally stored site data on exit pref.
  BooleanPrefMember clear_site_data_on_exit_;

  // Block all third party cookies.
  BooleanPrefMember block_third_party_cookies_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(CookieFilterPageView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OPTIONS_COOKIE_FILTER_PAGE_VIEW_H_

