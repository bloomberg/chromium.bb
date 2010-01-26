// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_OPTIONS_CONTENT_FILTER_PAGE_VIEW_H_
#define CHROME_BROWSER_VIEWS_OPTIONS_CONTENT_FILTER_PAGE_VIEW_H_

#include "chrome/browser/views/options/options_page_view.h"
#include "chrome/common/pref_member.h"
#include "views/controls/button/button.h"
#include "views/view.h"

namespace views {
class Label;
class NativeButton;
class RadioButton;
}
class PrefService;

////////////////////////////////////////////////////////////////////////////////
// ContentFilterPageView class is used to render Images, JavaScript and
// Plug-ins page in Content Settings dialog

class ContentFilterPageView : public OptionsPageView,
                              public views::ButtonListener {
 public:
  ContentFilterPageView(Profile* profile,
                        int label_message_id,
                        int allow_radio_message_id,
                        int block_radio_message_id);
  virtual ~ContentFilterPageView();

 protected:
  // views::ButtonListener implementation:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // OptionsPageView implementation:
  virtual void InitControlLayout();
  virtual void NotifyPrefChanged(const std::wstring* pref_name) {}

  // views::View overrides:
  virtual void Layout();

  // Signals that allowed radio button state has changed.
  virtual void OnAllowedChanged(bool allowed) {}

  // Signals that exceptions dialog should be shown.
  virtual void OnShowExceptionsDialog() {}

 private:
  // Controls for the content filter tab page.
  views::Label* caption_label_;
  views::RadioButton* allow_radio_;
  views::RadioButton* block_radio_;
  views::NativeButton* exceptions_button_;

  // Message ids for above UI controls.
  int label_message_id_;
  int allow_radio_message_id_;
  int block_radio_message_id_;

  DISALLOW_COPY_AND_ASSIGN(ContentFilterPageView);
};

#endif  // CHROME_BROWSER_VIEWS_OPTIONS_CONTENT_FILTER_PAGE_VIEW_H_

