// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/cookie_info_view.h"

#include <algorithm>

#include "base/i18n/time_formatting.h"
#include "base/message_loop.h"
#include "base/string16.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/cookies_tree_model.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "views/border.h"
#include "views/controls/label.h"
#include "views/controls/textfield/textfield.h"
#include "views/controls/tree/tree_view.h"

static const int kCookieInfoViewBorderSize = 1;
static const int kCookieInfoViewInsetSize = 3;

///////////////////////////////////////////////////////////////////////////////
// CookieInfoView, public:

CookieInfoView::CookieInfoView(bool editable_expiration_date)
    : name_label_(NULL),
      name_value_field_(NULL),
      content_label_(NULL),
      content_value_field_(NULL),
      domain_label_(NULL),
      domain_value_field_(NULL),
      path_label_(NULL),
      path_value_field_(NULL),
      send_for_label_(NULL),
      send_for_value_field_(NULL),
      created_label_(NULL),
      created_value_field_(NULL),
      expires_label_(NULL),
      expires_value_field_(NULL),
      expires_value_combobox_(NULL),
      expire_view_(NULL),
      editable_expiration_date_(editable_expiration_date),
      delegate_(NULL) {
}

CookieInfoView::~CookieInfoView() {
}

void CookieInfoView::SetCookie(
    const std::string& domain,
    const net::CookieMonster::CanonicalCookie& cookie) {
  name_value_field_->SetText(UTF8ToWide(cookie.Name()));
  content_value_field_->SetText(UTF8ToWide(cookie.Value()));
  domain_value_field_->SetText(UTF8ToWide(domain));
  path_value_field_->SetText(UTF8ToWide(cookie.Path()));
  created_value_field_->SetText(
      base::TimeFormatFriendlyDateAndTime(cookie.CreationDate()));

  std::wstring expire_text = cookie.DoesExpire() ?
      base::TimeFormatFriendlyDateAndTime(cookie.ExpiryDate()) :
      UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_EXPIRES_SESSION));

  if (editable_expiration_date_) {
    expire_combo_values_.clear();
    if (cookie.DoesExpire())
      expire_combo_values_.push_back(expire_text);
    expire_combo_values_.push_back(UTF16ToWide(
        l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_EXPIRES_SESSION)));
    expires_value_combobox_->ModelChanged();
    expires_value_combobox_->SetSelectedItem(0);
    expires_value_combobox_->SetEnabled(true);
    expires_value_combobox_->set_listener(this);
  } else {
    expires_value_field_->SetText(expire_text);
  }

  send_for_value_field_->SetText(cookie.IsSecure() ?
      UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_SENDFOR_SECURE)) :
      UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_SENDFOR_ANY)));
  EnableCookieDisplay(true);
  Layout();
}

void CookieInfoView::SetCookieString(const GURL& url,
                                     const std::string& cookie_line) {
  net::CookieMonster::ParsedCookie pc(cookie_line);
  net::CookieMonster::CanonicalCookie cookie(url, pc);
  SetCookie(pc.HasDomain() ? pc.Domain() : url.host(), cookie);
}


void CookieInfoView::ClearCookieDisplay() {
  std::wstring no_cookie_string =
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_NONESELECTED));
  name_value_field_->SetText(no_cookie_string);
  content_value_field_->SetText(no_cookie_string);
  domain_value_field_->SetText(no_cookie_string);
  path_value_field_->SetText(no_cookie_string);
  send_for_value_field_->SetText(no_cookie_string);
  created_value_field_->SetText(no_cookie_string);
  if (expires_value_field_)
    expires_value_field_->SetText(no_cookie_string);
  EnableCookieDisplay(false);
}

void CookieInfoView::EnableCookieDisplay(bool enabled) {
  name_value_field_->SetEnabled(enabled);
  content_value_field_->SetEnabled(enabled);
  domain_value_field_->SetEnabled(enabled);
  path_value_field_->SetEnabled(enabled);
  send_for_value_field_->SetEnabled(enabled);
  created_value_field_->SetEnabled(enabled);
  if (expires_value_field_)
    expires_value_field_->SetEnabled(enabled);
}

///////////////////////////////////////////////////////////////////////////////
// CookieInfoView, views::View overrides.

void CookieInfoView::ViewHierarchyChanged(bool is_add,
                                          views::View* parent,
                                          views::View* child) {
  if (is_add && child == this)
    Init();
}

///////////////////////////////////////////////////////////////////////////////
// CookieInfoView, views::Combobox::Listener overrides.

void CookieInfoView::ItemChanged(views::Combobox* combo_box,
                                 int prev_index,
                                 int new_index) {
  DCHECK(combo_box == expires_value_combobox_);
  if (delegate_)
    delegate_->ModifyExpireDate(new_index != 0);
}

///////////////////////////////////////////////////////////////////////////////
// CookieInfoView, ui::ComboboxModel overrides.
int CookieInfoView::GetItemCount() {
  return static_cast<int>(expire_combo_values_.size());
}

string16 CookieInfoView::GetItemAt(int index) {
  return WideToUTF16Hack(expire_combo_values_[index]);
}

void CookieInfoView::AddLabelRow(int layout_id, views::GridLayout* layout,
                                 views::View* label, views::View* value) {
  layout->StartRow(0, layout_id);
  layout->AddView(label);
  layout->AddView(value, 2, 1, views::GridLayout::FILL,
                  views::GridLayout::CENTER);
  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
}

void CookieInfoView::AddControlRow(int layout_id, views::GridLayout* layout,
                                 views::View* label, views::View* control) {
  layout->StartRow(0, layout_id);
  layout->AddView(label);
  layout->AddView(control, 1, 1);
  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
}

///////////////////////////////////////////////////////////////////////////////
// CookieInfoView, private:

void CookieInfoView::Init() {
  // Ensure we don't run this more than once and leak memory.
  DCHECK(!name_label_);

  SkColor border_color = color_utils::GetSysSkColor(COLOR_3DSHADOW);
  views::Border* border = views::Border::CreateSolidBorder(
      kCookieInfoViewBorderSize, border_color);
  set_border(border);

  name_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_NAME_LABEL));
  name_value_field_ = new views::Textfield;
  content_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_CONTENT_LABEL));
  content_value_field_ = new views::Textfield;
  domain_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_DOMAIN_LABEL));
  domain_value_field_ = new views::Textfield;
  path_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_PATH_LABEL));
  path_value_field_ = new views::Textfield;
  send_for_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_SENDFOR_LABEL));
  send_for_value_field_ = new views::Textfield;
  created_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_CREATED_LABEL));
  created_value_field_ = new views::Textfield;
  expires_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_COOKIES_COOKIE_EXPIRES_LABEL));
  if (editable_expiration_date_)
    expires_value_combobox_ = new views::Combobox(this);
  else
    expires_value_field_ = new views::Textfield;

  using views::GridLayout;
  using views::ColumnSet;

  GridLayout* layout = new GridLayout(this);
  layout->SetInsets(kCookieInfoViewInsetSize,
                    kCookieInfoViewInsetSize,
                    kCookieInfoViewInsetSize,
                    kCookieInfoViewInsetSize);
  SetLayoutManager(layout);

  int three_column_layout_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(three_column_layout_id);
  column_set->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::TRAILING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  AddLabelRow(three_column_layout_id, layout, name_label_, name_value_field_);
  AddLabelRow(three_column_layout_id, layout, content_label_,
              content_value_field_);
  AddLabelRow(three_column_layout_id, layout, domain_label_,
              domain_value_field_);
  AddLabelRow(three_column_layout_id, layout, path_label_, path_value_field_);
  AddLabelRow(three_column_layout_id, layout, send_for_label_,
              send_for_value_field_);
  AddLabelRow(three_column_layout_id, layout, created_label_,
              created_value_field_);

  if (editable_expiration_date_) {
    AddControlRow(three_column_layout_id, layout, expires_label_,
                  expires_value_combobox_);
  } else {
    AddLabelRow(three_column_layout_id, layout, expires_label_,
                expires_value_field_);
  }

  // Color these borderless text areas the same as the containing dialog.
  SkColor text_area_background = color_utils::GetSysSkColor(COLOR_3DFACE);
  // Now that the Textfields are in the view hierarchy, we can initialize them.
  name_value_field_->SetReadOnly(true);
  name_value_field_->RemoveBorder();
  name_value_field_->SetBackgroundColor(text_area_background);
  content_value_field_->SetReadOnly(true);
  content_value_field_->RemoveBorder();
  content_value_field_->SetBackgroundColor(text_area_background);
  domain_value_field_->SetReadOnly(true);
  domain_value_field_->RemoveBorder();
  domain_value_field_->SetBackgroundColor(text_area_background);
  path_value_field_->SetReadOnly(true);
  path_value_field_->RemoveBorder();
  path_value_field_->SetBackgroundColor(text_area_background);
  send_for_value_field_->SetReadOnly(true);
  send_for_value_field_->RemoveBorder();
  send_for_value_field_->SetBackgroundColor(text_area_background);
  created_value_field_->SetReadOnly(true);
  created_value_field_->RemoveBorder();
  created_value_field_->SetBackgroundColor(text_area_background);
  if (expires_value_field_) {
    expires_value_field_->SetReadOnly(true);
    expires_value_field_->RemoveBorder();
    expires_value_field_->SetBackgroundColor(text_area_background);
  }
}
