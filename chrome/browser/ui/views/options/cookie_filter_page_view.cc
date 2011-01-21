// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/options/cookie_filter_page_view.h"

#include "chrome/browser/content_settings/host_content_settings_map.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/options/show_options_url.h"
#include "chrome/browser/ui/views/options/cookies_view.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/button/checkbox.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"

CookieFilterPageView::CookieFilterPageView(Profile* profile)
    : ContentFilterPageView(profile, CONTENT_SETTINGS_TYPE_COOKIES),
      block_3rdparty_check_(NULL),
      clear_on_close_check_(NULL),
      show_cookies_button_(NULL) {
  clear_site_data_on_exit_.Init(prefs::kClearSiteDataOnExit,
                                profile->GetPrefs(), NULL);
  block_third_party_cookies_.Init(prefs::kBlockThirdPartyCookies,
                                  profile->GetPrefs(), this);
}

CookieFilterPageView::~CookieFilterPageView() {
}

///////////////////////////////////////////////////////////////////////////////
// CookieFilterPageView, ContentFilterPageView override:

void CookieFilterPageView::InitControlLayout() {
  ContentFilterPageView::InitControlLayout();

  using views::GridLayout;

  GridLayout* layout = static_cast<GridLayout*>(GetLayoutManager());
  const int single_column_set_id = 0;
  layout->AddPaddingRow(0, kUnrelatedControlLargeVerticalSpacing);

  block_3rdparty_check_ = new views::Checkbox(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_COOKIES_BLOCK_3RDPARTY_CHKBOX)));
  block_3rdparty_check_->set_listener(this);

  layout->StartRow(0, single_column_set_id);
  layout->AddView(block_3rdparty_check_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Now that this has been added to the view hierarchy, it's safe to call
  // SetChecked() on it.
  block_3rdparty_check_->SetChecked(
      profile()->GetHostContentSettingsMap()->BlockThirdPartyCookies());

  clear_on_close_check_ = new views::Checkbox(UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_COOKIES_CLEAR_WHEN_CLOSE_CHKBOX)));
  clear_on_close_check_->SetMultiLine(true);
  clear_on_close_check_->set_listener(this);

  layout->StartRow(0, single_column_set_id);
  layout->AddView(clear_on_close_check_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  show_cookies_button_ = new views::NativeButton(this,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_COOKIES_SHOW_COOKIES_BUTTON)));

  layout->StartRow(0, single_column_set_id);
  layout->AddView(show_cookies_button_, 1, 1, GridLayout::LEADING,
                  GridLayout::FILL);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  views::Link* flash_settings_link = new views::Link(
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_FLASH_STORAGE_SETTINGS)));
  flash_settings_link->SetController(this);

  layout->StartRow(0, single_column_set_id);
  layout->AddView(flash_settings_link, 1, 1, GridLayout::LEADING,
                  GridLayout::FILL);
}

///////////////////////////////////////////////////////////////////////////////
// CookieFilterPageView, OptionsPageView implementation:

void CookieFilterPageView::NotifyPrefChanged(const std::string* pref_name) {
  if (!pref_name || *pref_name == prefs::kClearSiteDataOnExit) {
    clear_on_close_check_->SetChecked(
        clear_site_data_on_exit_.GetValue());
  }
  if (!pref_name || *pref_name == prefs::kBlockThirdPartyCookies) {
    block_3rdparty_check_->SetChecked(
        block_third_party_cookies_.GetValue());
    block_3rdparty_check_->SetEnabled(
        !block_third_party_cookies_.IsManaged());
  }
}

///////////////////////////////////////////////////////////////////////////////
// CookieFilterPageView, views::ButtonListener implementation:

void CookieFilterPageView::ButtonPressed(views::Button* sender,
                                         const views::Event& event) {
  HostContentSettingsMap* settings_map = profile()->GetHostContentSettingsMap();
  if (sender == block_3rdparty_check_) {
    settings_map->SetBlockThirdPartyCookies(block_3rdparty_check_->checked());
  } else if (sender == clear_on_close_check_) {
    clear_site_data_on_exit_.SetValue(clear_on_close_check_->checked());
  } else if (sender == show_cookies_button_) {
    UserMetricsRecordAction(UserMetricsAction("Options_ShowCookies"), NULL);
    CookiesView::ShowCookiesWindow(profile());
  } else {
    ContentFilterPageView::ButtonPressed(sender, event);
  }
}

///////////////////////////////////////////////////////////////////////////////
// CookieFilterPageView, views::LinkController implementation:

void CookieFilterPageView::LinkActivated(views::Link* source, int event_flags) {
  browser::ShowOptionsURL(
      profile(),
      GURL(l10n_util::GetStringUTF8(IDS_FLASH_STORAGE_URL)));
}
