// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/options/cookie_filter_page_view.h"

#include "app/gfx/canvas.h"
#include "app/gfx/native_theme_win.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/views/options/cookies_view.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/button/radio_button.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

CookieFilterPageView::CookieFilterPageView(Profile* profile)
    : allow_radio_(NULL),
      ask_radio_(NULL),
      block_radio_(NULL),
      exceptions_button_(NULL),
      block_3rdparty_check_(NULL),
      clear_on_close_check_(NULL),
      show_cookies_button_(NULL),
      OptionsPageView(profile) {
}

CookieFilterPageView::~CookieFilterPageView() {
}

///////////////////////////////////////////////////////////////////////////////
// CookieFilterPageView, OptionsPageView implementation:
void CookieFilterPageView::InitControlLayout() {
  using views::GridLayout;

  GridLayout* layout = new GridLayout(this);
  layout->SetInsets(5, 5, 5, 5);
  SetLayoutManager(layout);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  const int single_column_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(single_column_set_id);
  column_set->AddPaddingColumn(0, kRelatedControlVerticalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  views::Label* title_label = new views::Label(
      l10n_util::GetString(IDS_MODIFY_COOKIE_STORING_LABEL));
  title_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  title_label->SetMultiLine(true);

  layout->StartRow(0, single_column_set_id);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  const int radio_button_group = 0;
  allow_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_COOKIES_ALLOW_RADIO), radio_button_group);
  allow_radio_->set_listener(this);
  allow_radio_->SetMultiLine(true);

  ask_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_COOKIES_ASK_EVERY_TIME_RADIO),
      radio_button_group);
  ask_radio_->set_listener(this);
  ask_radio_->SetMultiLine(true);

  block_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_COOKIES_BLOCK_RADIO), radio_button_group);
  block_radio_->set_listener(this);
  block_radio_->SetMultiLine(true);

  layout->StartRow(0, single_column_set_id);
  layout->AddView(allow_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_set_id);
  layout->AddView(ask_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_set_id);
  layout->AddView(block_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  const HostContentSettingsMap* settings_map =
      profile()->GetHostContentSettingsMap();
  ContentSetting default_setting =
      settings_map->GetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES);
  // Now that these have been added to the view hierarchy, it's safe to call
  // SetChecked() on them.
  if (default_setting == CONTENT_SETTING_ALLOW) {
    allow_radio_->SetChecked(true);
  } else if (default_setting == CONTENT_SETTING_BLOCK) {
    block_radio_->SetChecked(true);
  } else {
    DCHECK(default_setting == CONTENT_SETTING_ASK);
    ask_radio_->SetChecked(true);
  }

  exceptions_button_ = new views::NativeButton(this,
      l10n_util::GetString(IDS_COOKIES_EXCEPTIONS_BUTTON));

  layout->StartRow(0, single_column_set_id);
  layout->AddView(exceptions_button_, 1, 1, GridLayout::LEADING,
                  GridLayout::FILL);
  layout->AddPaddingRow(0, kUnrelatedControlLargeVerticalSpacing);

  block_3rdparty_check_ = new views::Checkbox(
      l10n_util::GetString(IDS_COOKIES_BLOCK_3RDPARTY_CHKBOX));
  block_3rdparty_check_->set_listener(this);

  layout->StartRow(0, single_column_set_id);
  layout->AddView(block_3rdparty_check_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Now that this has been added to the view hierarchy, it's safe to call
  // SetChecked() on it.
  block_3rdparty_check_->SetChecked(settings_map->BlockThirdPartyCookies());

  clear_on_close_check_ = new views::Checkbox(
      l10n_util::GetString(IDS_COOKIES_CLEAR_WHEN_CLOSE_CHKBOX));
  clear_on_close_check_->set_listener(this);

  // TODO(pkasting): Set clear-on-close checkbox to be checked or not.

  layout->StartRow(0, single_column_set_id);
  layout->AddView(clear_on_close_check_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  show_cookies_button_ = new views::NativeButton(this,
      l10n_util::GetString(IDS_COOKIES_SHOW_COOKIES_BUTTON));

  layout->StartRow(0, single_column_set_id);
  layout->AddView(show_cookies_button_, 1, 1, GridLayout::LEADING,
                  GridLayout::FILL);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  views::Link* flash_settings_link = new views::Link(
      l10n_util::GetString(IDS_FLASH_STORAGE_SETTINGS));
  flash_settings_link->SetController(this);

  layout->StartRow(0, single_column_set_id);
  layout->AddView(flash_settings_link, 1, 1, GridLayout::LEADING,
                  GridLayout::FILL);
}

///////////////////////////////////////////////////////////////////////////////
// CookieFilterPageView, views::ButtonListener implementation:

void CookieFilterPageView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  HostContentSettingsMap* settings_map = profile()->GetHostContentSettingsMap();
  if (sender == allow_radio_) {
    settings_map->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES,
                                           CONTENT_SETTING_ALLOW);
  } else if (sender == ask_radio_) {
    settings_map->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES,
                                           CONTENT_SETTING_ASK);
  } else if (sender == block_radio_) {
    settings_map->SetDefaultContentSetting(CONTENT_SETTINGS_TYPE_COOKIES,
                                           CONTENT_SETTING_BLOCK);
  } else if (sender == exceptions_button_) {
    // TODO(pkasting): Show exceptions dialog.
  } else if (sender == block_3rdparty_check_) {
    settings_map->SetBlockThirdPartyCookies(block_3rdparty_check_->checked());
  } else if (sender == clear_on_close_check_) {
    // TODO(pkasting): Set clear-on-close setting.
  } else {
    DCHECK_EQ(sender, show_cookies_button_);
    UserMetricsRecordAction("Options_ShowCookies", NULL);
    CookiesView::ShowCookiesWindow(profile());
  }
}

///////////////////////////////////////////////////////////////////////////////
// CookieFilterPageView, views::LinkController implementation:

void CookieFilterPageView::LinkActivated(views::Link* source, int event_flags) {
  // We open a new browser window so the Options dialog doesn't get lost
  // behind other windows.
  Browser* browser = Browser::Create(profile());
  browser->OpenURL(GURL(l10n_util::GetString(IDS_FLASH_STORAGE_URL)), GURL(),
                   NEW_WINDOW, PageTransition::LINK);
}
