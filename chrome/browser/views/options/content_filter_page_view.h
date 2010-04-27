// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_CONTENT_FILTER_PAGE_VIEW_H_
#define CHROME_BROWSER_VIEWS_OPTIONS_CONTENT_FILTER_PAGE_VIEW_H_

#include "chrome/browser/views/options/options_page_view.h"
#include "chrome/common/content_settings_types.h"
#include "views/controls/button/button.h"

namespace views {
class Label;
class NativeButton;
class RadioButton;
}
class PrefService;

////////////////////////////////////////////////////////////////////////////////
// The ContentFilterPageView class is used to render the Images, JavaScript,
// Pop-ups and Location pages in the Content Settings window.

class ContentFilterPageView : public OptionsPageView,
                              public views::ButtonListener {
 public:
  ContentFilterPageView(Profile* profile, ContentSettingsType content_type);
  virtual ~ContentFilterPageView();

 protected:
  // OptionsPageView implementation:
  virtual void InitControlLayout();

  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

 private:
  ContentSettingsType content_type_;

  // Controls for the content filter tab page.
  views::RadioButton* allow_radio_;
  views::RadioButton* ask_radio_;
  views::RadioButton* block_radio_;
  views::NativeButton* exceptions_button_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ContentFilterPageView);
};

#endif  // CHROME_BROWSER_VIEWS_OPTIONS_CONTENT_FILTER_PAGE_VIEW_H_
