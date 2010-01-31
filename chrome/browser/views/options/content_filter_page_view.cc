// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/options/content_filter_page_view.h"

#include "app/gfx/canvas.h"
#include "app/gfx/native_theme_win.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/host_content_settings_map.h"
#include "chrome/browser/profile.h"
#include "chrome/common/pref_names.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/button/radio_button.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

ContentFilterPageView::ContentFilterPageView(Profile* profile,
                                             ContentSettingsType content_type)
    : OptionsPageView(profile),
      content_type_(content_type),
      allow_radio_(NULL),
      block_radio_(NULL),
      exceptions_button_(NULL) {
}

ContentFilterPageView::~ContentFilterPageView() {
}

////////////////////////////////////////////////////////////////////////////////
// ContentFilterPageView, OptionsPageView implementation:
void ContentFilterPageView::InitControlLayout() {
  // Make sure we don't leak memory by calling this more than once.
  DCHECK(!exceptions_button_);
  using views::GridLayout;

  GridLayout* layout = new GridLayout(this);
  layout->SetInsets(5, 5, 5, 5);
  SetLayoutManager(layout);

  const int single_column_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(single_column_set_id);
  column_set->AddPaddingColumn(0, kRelatedControlVerticalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  static const int kTitleIDs[CONTENT_SETTINGS_NUM_TYPES] = {
    0,  // This dialog isn't used for cookies.
    IDS_IMAGES_SETTING_LABEL,
    IDS_JS_SETTING_LABEL,
    IDS_PLUGIN_SETTING_LABEL,
    IDS_POPUP_SETTING_LABEL,
  };
  DCHECK_EQ(arraysize(kTitleIDs),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
  views::Label* title_label = new views::Label(
      l10n_util::GetString(kTitleIDs[content_type_]));
  title_label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  title_label->SetMultiLine(true);

  layout->StartRow(0, single_column_set_id);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  static const int kAllowIDs[CONTENT_SETTINGS_NUM_TYPES] = {
    0,  // This dialog isn't used for cookies.
    IDS_IMAGES_LOAD_RADIO,
    IDS_JS_ALLOW_RADIO,
    IDS_PLUGIN_LOAD_RADIO,
    IDS_POPUP_ALLOW_RADIO,
  };
  DCHECK_EQ(arraysize(kAllowIDs),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
  const int radio_button_group = 0;
  allow_radio_ = new views::RadioButton(
      l10n_util::GetString(kAllowIDs[content_type_]), radio_button_group);
  allow_radio_->set_listener(this);
  allow_radio_->SetMultiLine(true);

  static const int kBlockIDs[CONTENT_SETTINGS_NUM_TYPES] = {
    0,  // This dialog isn't used for cookies.
    IDS_IMAGES_NOLOAD_RADIO,
    IDS_JS_DONOTALLOW_RADIO,
    IDS_PLUGIN_NOLOAD_RADIO,
    IDS_POPUP_BLOCK_RADIO,
  };
  DCHECK_EQ(arraysize(kBlockIDs),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
  block_radio_ = new views::RadioButton(
      l10n_util::GetString(kBlockIDs[content_type_]), radio_button_group);
  block_radio_->set_listener(this);
  block_radio_->SetMultiLine(true);

  layout->StartRow(0, single_column_set_id);
  layout->AddView(allow_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  layout->StartRow(0, single_column_set_id);
  layout->AddView(block_radio_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  ContentSetting default_setting = profile()->GetHostContentSettingsMap()->
      GetDefaultContentSetting(content_type_);
  // Now that these have been added to the view hierarchy, it's safe to call
  // SetChecked() on them.
  if (default_setting == CONTENT_SETTING_ALLOW) {
    allow_radio_->SetChecked(true);
  } else {
    DCHECK(default_setting == CONTENT_SETTING_BLOCK);
    block_radio_->SetChecked(true);
  }

  exceptions_button_ = new views::NativeButton(this,
      l10n_util::GetString(IDS_COOKIES_EXCEPTIONS_BUTTON));

  layout->StartRow(0, single_column_set_id);
  layout->AddView(exceptions_button_, 1, 1, GridLayout::LEADING,
                  GridLayout::FILL);
}

///////////////////////////////////////////////////////////////////////////////
// ContentFilterPageView, views::ButtonListener implementation:

void ContentFilterPageView::ButtonPressed(views::Button* sender,
                                          const views::Event& event) {
  if (sender == exceptions_button_) {
    // TODO(pkasting): Show exceptions dialog
    return;
  }

  DCHECK((sender == allow_radio_) || (sender == block_radio_));
  profile()->GetHostContentSettingsMap()->SetDefaultContentSetting(
      content_type_,
      allow_radio_->checked() ? CONTENT_SETTING_ALLOW : CONTENT_SETTING_BLOCK);
}
