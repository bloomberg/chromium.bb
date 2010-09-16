// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/cellular_config_view.h"

#include "app/l10n_util.h"
#include "base/i18n/time_formatting.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "chrome/common/time_format.h"
#include "grit/generated_resources.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/button/native_button.h"
#include "views/controls/label.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"

namespace {

enum PlanPurchaseType {
  UNKNOWN,
  NO_PURCHASE,                // No purchase happened.
  PURCHASED_DATA,             // Purchased limited data plan.
  PURCHASED_UNLIMITED_DATA    // Purchased unlimited data plan.
};

struct PlanDetails {
  PlanPurchaseType last_purchase_type;
  base::Time last_purchase_time;

  int64 purchased_data;
  base::TimeDelta purchased_time;

  int64 remaining_data;
  base::TimeDelta remaining_time;
};

// TODO(xiyuan): Get real data from libcros when it's ready.
// Get plan details at the time being called.
void GetPlanDetails(const chromeos::CellularNetwork& cellular,
                    PlanDetails* details) {
  // Free 5M 30day plan.
  details->last_purchase_type = UNKNOWN;
  details->last_purchase_time = base::Time::Now();
  details->purchased_data = 5 * 1024 * 1024;
  details->purchased_time = base::TimeDelta::FromDays(30);
  details->remaining_data = 2 * 1024 * 1024;
  details->remaining_time = base::TimeDelta::FromDays(29);
}

}  // namespace

namespace chromeos {

CellularConfigView::CellularConfigView(NetworkConfigView* parent,
                                       const CellularNetwork& cellular)
    : parent_(parent),
      cellular_(cellular),
      purchase_info_(NULL),
      purchase_more_button_(NULL),
      remaining_data_info_(NULL),
      expiration_info_(NULL),
      show_notification_checkbox_(NULL),
      autoconnect_checkbox_(NULL),
      customer_support_link_(NULL) {
  Init();
}

void CellularConfigView::ButtonPressed(views::Button* button,
    const views::Event& event) {
  if (button == purchase_more_button_) {
    // TODO(xiyuan): Purchase more...
  }
}

void CellularConfigView::LinkActivated(views::Link* source, int event_flags) {
  if (source == customer_support_link_) {
    // TODO(xiyuan): Find out where to go.
  }
}

bool CellularConfigView::Save() {
  // Save auto-connect here.
  bool auto_connect = autoconnect_checkbox_->checked();
  if (auto_connect != cellular_.auto_connect()) {
    cellular_.set_auto_connect(auto_connect);
    CrosLibrary::Get()->GetNetworkLibrary()->SaveCellularNetwork(cellular_);
  }
  return true;
}

void CellularConfigView::Init() {
  views::GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  purchase_info_ = new views::Label();
  purchase_more_button_ = new views::NativeButton(this, l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PURCHASE_MORE));
  views::Label* data_remaining_label = new views::Label(l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_DATA_REMAINING));
  remaining_data_info_ = new views::Label();
  views::Label* expires_label = new views::Label(l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_EXPIRES));
  expiration_info_ = new views::Label();
  show_notification_checkbox_ = new views::Checkbox(l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SHOW_MOBILE_NOTIFICATION));
  autoconnect_checkbox_ = new views::Checkbox(l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_AUTO_CONNECT));
  customer_support_link_ = new views::Link(l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_CUSTOMER_SUPPORT));

  data_remaining_label->SetFont(data_remaining_label->font().DeriveFont(0,
       gfx::Font::BOLD));
  expires_label->SetFont(data_remaining_label->font());

  const int kColumnSetId = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(kColumnSetId);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(views::GridLayout::TRAILING, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, kColumnSetId);
  layout->AddView(purchase_info_, 2, 1);
  layout->AddView(purchase_more_button_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, kColumnSetId);
  layout->AddView(data_remaining_label);
  layout->AddView(remaining_data_info_, 2, 1);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, kColumnSetId);
  layout->AddView(expires_label);
  layout->AddView(expiration_info_, 2, 1);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, kColumnSetId);
  layout->AddView(show_notification_checkbox_, 3, 1);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, kColumnSetId);
  layout->AddView(autoconnect_checkbox_, 3, 1);
  layout->AddPaddingRow(0, kUnrelatedControlVerticalSpacing);

  layout->StartRow(0, kColumnSetId);
  layout->AddView(customer_support_link_, 3, 1,
      views::GridLayout::LEADING, views::GridLayout::TRAILING);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  Update();
}

void CellularConfigView::Update() {
  autoconnect_checkbox_->SetChecked(cellular_.auto_connect());

  PlanDetails details;
  GetPlanDetails(cellular_, &details);

  switch (details.last_purchase_type) {
    case NO_PURCHASE:
      purchase_info_->SetText(l10n_util::GetStringF(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_RECEIVED_FREE_DATA,
          UTF16ToWide(FormatBytes(details.purchased_data,
                      GetByteDisplayUnits(details.purchased_data),
                      true)),
          base::TimeFormatFriendlyDate(details.last_purchase_time)));
      remaining_data_info_->SetText(
          UTF16ToWide(FormatBytes(details.remaining_data,
                      GetByteDisplayUnits(details.remaining_data),
                      true)));
      expiration_info_->SetText(UTF16ToWide(
          TimeFormat::TimeRemaining(details.remaining_time)));
      break;
    case PURCHASED_DATA:
      purchase_info_->SetText(l10n_util::GetStringF(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PURCHASE_DATA,
          UTF16ToWide(FormatBytes(details.purchased_data,
                      GetByteDisplayUnits(details.purchased_data),
                      true)),
          base::TimeFormatFriendlyDate(details.last_purchase_time)));
      remaining_data_info_->SetText(
          UTF16ToWide(FormatBytes(details.remaining_data,
                      GetByteDisplayUnits(details.remaining_data),
                      true)));
      expiration_info_->SetText(UTF16ToWide(
          TimeFormat::TimeRemaining(details.remaining_time)));
      break;
    case PURCHASED_UNLIMITED_DATA:
      purchase_info_->SetText(l10n_util::GetStringF(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_PURCHASE_UNLIMITED_DATA,
          UTF16ToWide(FormatBytes(details.purchased_data,
                      GetByteDisplayUnits(details.purchased_data),
                      true)),
          base::TimeFormatFriendlyDate(details.last_purchase_time)));
      remaining_data_info_->SetText(l10n_util::GetString(
          IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_UNLIMITED));
      expiration_info_->SetText(UTF16ToWide(
          TimeFormat::TimeRemaining(details.remaining_time)));
      break;
    case UNKNOWN: {
      // TODO(xiyuan): Remove this when underlying data is provided.
      const wchar_t kPlanType[] = L"Purchased plan: <Not yet implemented>";
      const wchar_t kNotImplemented[] = L"<Not yet implemented>";
      purchase_info_->SetText(kPlanType);
      remaining_data_info_->SetText(kNotImplemented);
      expiration_info_->SetText(kNotImplemented);
      break;
    }
    default:
      NOTREACHED() << "Unknown mobile plan purchase type.";
      break;
  }

  Layout();
}

}  // namespace chromeos
