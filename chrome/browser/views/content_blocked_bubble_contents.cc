// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/views/content_blocked_bubble_contents.h"

#include "app/l10n_util.h"
#include "chrome/browser/views/info_bubble.h"
#include "chrome/browser/views/options/content_settings_window_view.h"
#include "grit/generated_resources.h"
#include "views/controls/button/native_button.h"
#include "views/controls/button/radio_button.h"
#include "views/controls/label.h"
#include "views/controls/separator.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"

ContentBlockedBubbleContents::ContentBlockedBubbleContents(
    ContentSettingsType content_type,
    const std::wstring& host,
    Profile* profile)
    : content_type_(content_type),
      host_(host),
      profile_(profile),
      info_bubble_(NULL),
      allow_radio_(NULL),
      block_radio_(NULL),
      close_button_(NULL),
      manage_link_(NULL) {
}

ContentBlockedBubbleContents::~ContentBlockedBubbleContents() {
}

void ContentBlockedBubbleContents::ViewHierarchyChanged(bool is_add,
                                                        View* parent,
                                                        View* child) {
  if (is_add && (child == this))
    InitControlLayout();
}

void ContentBlockedBubbleContents::ButtonPressed(views::Button* sender,
                                                 const views::Event& event) {
  if (sender == close_button_) {
    info_bubble_->Close();  // CAREFUL: This deletes us.
    return;
  }

  // TODO(pkasting): Set block state for this host and content type.
}

void ContentBlockedBubbleContents::LinkActivated(views::Link* source,
                                                 int event_flags) {
  if (source == manage_link_) {
    ContentSettingsWindowView::Show(content_type_, profile_);
    // CAREFUL: Showing the settings window activates it, which deactivates the
    // info bubble, which causes it to close, which deletes us.
    return;
  }

  // TODO(pkasting): A popup link was clicked, show the corresponding popup.
}

void ContentBlockedBubbleContents::InitControlLayout() {
  using views::GridLayout;

  GridLayout* layout = new views::GridLayout(this);
  SetLayoutManager(layout);

  const int single_column_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(single_column_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);

  static const int kTitleIDs[] = {
    IDS_BLOCKED_COOKIES_TITLE,
    IDS_BLOCKED_IMAGES_TITLE,
    IDS_BLOCKED_JAVASCRIPT_TITLE,
    IDS_BLOCKED_PLUGINS_TITLE,
    IDS_BLOCKED_POPUPS_TITLE,
  };
  DCHECK_EQ(arraysize(kTitleIDs),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
  views::Label* title_label =
      new views::Label(l10n_util::GetString(kTitleIDs[content_type_]));

  layout->StartRow(0, single_column_set_id);
  layout->AddView(title_label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  if (content_type_ == CONTENT_SETTINGS_TYPE_POPUPS) {
    /* TODO(pkasting): Show list of blocked popups as clickable links.
    for (size_t i = 0; i < num_popups; ++i) {
      if (i != 0)
        layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
      layout->StartRow(0, single_column_set_id);
      layout->AddView(popup_link);
    }
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
    */

    views::Separator* separator = new views::Separator;
    layout->StartRow(0, single_column_set_id);
    layout->AddView(separator);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  }

  if (content_type_ != CONTENT_SETTINGS_TYPE_COOKIES) {
    static const int kAllowIDs[] = {
      0,  // Not displayed for cookies
      IDS_BLOCKED_IMAGES_UNBLOCK,
      IDS_BLOCKED_JAVASCRIPT_UNBLOCK,
      IDS_BLOCKED_PLUGINS_UNBLOCK,
      IDS_BLOCKED_POPUPS_UNBLOCK,
    };
    DCHECK_EQ(arraysize(kAllowIDs),
              static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
    const int radio_button_group = 0;
    allow_radio_ = new views::RadioButton(
        l10n_util::GetStringF(kAllowIDs[content_type_], host_),
        radio_button_group);
    allow_radio_->set_listener(this);

    static const int kBlockIDs[] = {
      0,  // Not displayed for cookies
      IDS_BLOCKED_IMAGES_NO_ACTION,
      IDS_BLOCKED_JAVASCRIPT_NO_ACTION,
      IDS_BLOCKED_PLUGINS_NO_ACTION,
      IDS_BLOCKED_POPUPS_NO_ACTION,
    };
    DCHECK_EQ(arraysize(kAllowIDs),
              static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
    block_radio_ = new views::RadioButton(
        l10n_util::GetString(kBlockIDs[content_type_]), radio_button_group);
    block_radio_->set_listener(this);

    layout->StartRow(0, single_column_set_id);
    layout->AddView(allow_radio_);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
    layout->StartRow(0, single_column_set_id);
    layout->AddView(block_radio_);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

    // Now that this has been added to the view hierarchy, it's safe to call
    // SetChecked() on it.
    block_radio_->SetChecked(true);

    views::Separator* separator = new views::Separator;
    layout->StartRow(0, single_column_set_id);
    layout->AddView(separator, 1, 1, GridLayout::FILL, GridLayout::FILL);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  }

  const int double_column_set_id = 1;
  views::ColumnSet* double_column_set =
      layout->AddColumnSet(double_column_set_id);
  double_column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  double_column_set->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  static const int kLinkIDs[] = {
    IDS_BLOCKED_COOKIES_LINK,
    IDS_BLOCKED_IMAGES_LINK,
    IDS_BLOCKED_JAVASCRIPT_LINK,
    IDS_BLOCKED_PLUGINS_LINK,
    IDS_BLOCKED_POPUPS_LINK,
  };
  DCHECK_EQ(arraysize(kLinkIDs),
            static_cast<size_t>(CONTENT_SETTINGS_NUM_TYPES));
  manage_link_ = new views::Link(l10n_util::GetString(kLinkIDs[content_type_]));
  manage_link_->SetController(this);

  layout->StartRow(0, double_column_set_id);
  layout->AddView(manage_link_);

  close_button_ =
      new views::NativeButton(this, l10n_util::GetString(IDS_CLOSE));
  layout->AddView(close_button_);
}
