// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/options/cookie_filter_page_view.h"

#include "app/gfx/canvas.h"
#include "app/gfx/native_theme_win.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
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

static const int kAllowRadioGroup = 1;
static const int kFilterPageInset = 5;

CookieFilterPageView::CookieFilterPageView(Profile* profile)
    : caption_label_(NULL),
      allow_radio_(NULL),
      ask_radio_(NULL),
      block_radio_(NULL),
      exceptions_button_(NULL),
      block_3rdparty_check_(NULL),
      clear_on_close_check_(NULL),
      show_cookies_button_(NULL),
      flash_settings_link_(NULL),
      OptionsPageView(profile) {
}

CookieFilterPageView::~CookieFilterPageView() {
}

///////////////////////////////////////////////////////////////////////////////
// CookieFilterPageView, views::ButtonListener implementation:

void CookieFilterPageView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == allow_radio_) {
    SetAllowCookies(COOKIES_ALLOWED_ALLOW);
  } else if (sender == ask_radio_) {
    SetAllowCookies(COOKIES_ALLOWED_ASK);
  } else if (sender == block_radio_) {
    SetAllowCookies(COOKIES_ALLOWED_BLOCK);
  } else if (sender == exceptions_button_) {
    ShowCookieExceptionsDialog();
  } else if (sender == block_3rdparty_check_) {
    SetBlock3rdPartyCookies(block_3rdparty_check_->checked());
  } else if (sender == clear_on_close_check_) {
    SetBlock3rdPartyCookies(block_3rdparty_check_->checked());
  } else if (sender == show_cookies_button_) {
    ShowCookieManagerDialog();
  }
}

///////////////////////////////////////////////////////////////////////////////
// CookieFilterPageView, views::LinkController implementation:
void CookieFilterPageView::LinkActivated(views::Link* source,
                                         int event_flags) {
  if (source == flash_settings_link_)
    OpenFlashSettingsDialog();
}

// TODO(zelidrag): Hook setters for preference settings here:
void CookieFilterPageView::SetAllowCookies(CookiesAllowed allowed) {
}

void CookieFilterPageView::ShowCookieExceptionsDialog() {
}

void CookieFilterPageView::SetBlock3rdPartyCookies(bool block) {
}

void CookieFilterPageView::SetClearCookiesOnClose(bool clear) {
}

void CookieFilterPageView::OpenFlashSettingsDialog() {
}

void CookieFilterPageView::ShowCookieManagerDialog() {
  UserMetricsRecordAction("Options_ShowCookies", NULL);
  CookiesView::ShowCookiesWindow(profile());
}

///////////////////////////////////////////////////////////////////////////////
// CookieFilterPageView, OptionsPageView implementation:
void CookieFilterPageView::InitControlLayout() {
  using views::GridLayout;
  using views::ColumnSet;

  caption_label_ = new views::Label(
      l10n_util::GetString(IDS_MODIFY_COOKIE_STORING_LABEL));
  caption_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  caption_label_->SetMultiLine(true);

  allow_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_COOKIES_ALLOW_RADIO),
      kAllowRadioGroup);
  allow_radio_->set_listener(this);
  allow_radio_->SetMultiLine(true);

  ask_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_COOKIES_ASK_EVERY_TIME_RADIO),
      kAllowRadioGroup);
  ask_radio_->set_listener(this);
  ask_radio_->SetMultiLine(true);

  block_radio_ = new views::RadioButton(
      l10n_util::GetString(IDS_COOKIES_BLOCK_RADIO),
      kAllowRadioGroup);
  block_radio_->set_listener(this);
  block_radio_->SetMultiLine(true);

  exceptions_button_ = new views::NativeButton(this,
      l10n_util::GetString(IDS_COOKIES_EXCEPTIONS_BUTTON));

  block_3rdparty_check_ = new views::Checkbox(
      l10n_util::GetString(IDS_COOKIES_BLOCK_3RDPARTY_CHKBOX));
  block_3rdparty_check_->set_listener(this);

  clear_on_close_check_ = new views::Checkbox(
      l10n_util::GetString(IDS_COOKIES_CLEAR_WHEN_CLOSE_CHKBOX));
  clear_on_close_check_->set_listener(this);

  show_cookies_button_ = new views::NativeButton(this,
      l10n_util::GetString(IDS_COOKIES_SHOW_COOKIES_BUTTON));

  flash_settings_link_ = new views::Link(
      l10n_util::GetString(IDS_FLASH_STORAGE_SETTINGS));
  flash_settings_link_->SetController(this);

  GridLayout* layout = new GridLayout(this);
  layout->SetInsets(kFilterPageInset, kFilterPageInset, kFilterPageInset,
                    kFilterPageInset);
  SetLayoutManager(layout);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  const int label_view_set_id = 0;
  ColumnSet* label_column_set = layout->AddColumnSet(label_view_set_id);
  label_column_set->AddPaddingColumn(0, kRelatedControlVerticalSpacing);
  label_column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                              GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, label_view_set_id);
  layout->AddView(caption_label_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  const int radio_view_set_id = 1;
  ColumnSet* radio_column_set = layout->AddColumnSet(radio_view_set_id);
  radio_column_set->AddPaddingColumn(0, kRelatedControlVerticalSpacing);
  radio_column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                              GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, radio_view_set_id);
  layout->AddView(allow_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, radio_view_set_id);
  layout->AddView(ask_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, radio_view_set_id);
  layout->AddView(block_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  const int exceptions_button_view_set_id = 2;
  ColumnSet* button_column_set = layout->AddColumnSet(
      exceptions_button_view_set_id);
  button_column_set->AddPaddingColumn(0, kRelatedControlVerticalSpacing);
  button_column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                               GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, exceptions_button_view_set_id);
  layout->AddView(exceptions_button_);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  const int check_view_set_id = 3;
  ColumnSet* check_column_set = layout->AddColumnSet(check_view_set_id);
  check_column_set->AddPaddingColumn(0, kRelatedControlVerticalSpacing);
  check_column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                              GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, check_view_set_id);
  layout->AddView(block_3rdparty_check_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, check_view_set_id);
  layout->AddView(clear_on_close_check_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  const int mgr_button_view_set_id = 4;
  ColumnSet* mgr_button_column_set =
      layout->AddColumnSet(mgr_button_view_set_id);
  mgr_button_column_set->AddPaddingColumn(0, kRelatedControlVerticalSpacing);
  mgr_button_column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                                   GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, mgr_button_view_set_id);
  layout->AddView(show_cookies_button_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  const int link_view_set_id = 5;
  ColumnSet* link_column_set = layout->AddColumnSet(link_view_set_id);
  link_column_set->AddPaddingColumn(0, kRelatedControlVerticalSpacing);
  link_column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                             GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, link_view_set_id);
  layout->AddView(flash_settings_link_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
}


