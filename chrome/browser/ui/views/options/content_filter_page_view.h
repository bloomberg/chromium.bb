// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_OPTIONS_CONTENT_FILTER_PAGE_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_OPTIONS_CONTENT_FILTER_PAGE_VIEW_H_
#pragma once

#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/ui/views/options/options_page_view.h"
#include "chrome/common/content_settings_types.h"
#include "content/common/notification_registrar.h"
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

  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);
 protected:
  // Updates the state of the UI. UpdateView is called when the content filter
  // page is initialized and after the content setting changed (either the
  // value or the is managed state).
  virtual void UpdateView();

  virtual void NotifyContentSettingsChanged(
      const ContentSettingsDetails* details);

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

  NotificationRegistrar registrar_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(ContentFilterPageView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_OPTIONS_CONTENT_FILTER_PAGE_VIEW_H_
