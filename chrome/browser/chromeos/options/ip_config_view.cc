// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/ip_config_view.h"

#include "app/l10n_util.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/label.h"
#include "views/controls/textfield/textfield.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

namespace chromeos {

IPConfigView::IPConfigView(const std::string& device_path)
    : device_path_(device_path),
      ip_configs_(),
      address_textfield_(NULL),
      netmask_textfield_(NULL),
      gateway_textfield_(NULL),
      dnsserver_textfield_(NULL) {
  Init();
  RefreshData();
}

void IPConfigView::RefreshData() {
  NetworkIPConfigVector ipconfigs =
      CrosLibrary::Get()->GetNetworkLibrary()->GetIPConfigs(device_path_);
  for (NetworkIPConfigVector::const_iterator it = ipconfigs.begin();
       it != ipconfigs.end(); ++it) {
    const NetworkIPConfig& ipconfig = *it;
    address_textfield_->SetText(ASCIIToUTF16(ipconfig.address));
    netmask_textfield_->SetText(ASCIIToUTF16(ipconfig.netmask));
    gateway_textfield_->SetText(ASCIIToUTF16(ipconfig.gateway));
    dnsserver_textfield_->SetText(ASCIIToUTF16(ipconfig.name_servers));
  }
}

void IPConfigView::Init() {
  views::GridLayout* layout = CreatePanelGridLayout(this);
  SetLayoutManager(layout);

  int column_view_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_view_set_id);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 200);

  layout->StartRow(0, column_view_set_id);
  layout->AddView(new views::Label(l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_ADDRESS)));
  address_textfield_ = new views::Textfield(views::Textfield::STYLE_DEFAULT);
  address_textfield_->SetEnabled(false);
  layout->AddView(address_textfield_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, column_view_set_id);
  layout->AddView(new views::Label(l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_SUBNETMASK)));
  netmask_textfield_ = new views::Textfield(views::Textfield::STYLE_DEFAULT);
  netmask_textfield_->SetEnabled(false);
  layout->AddView(netmask_textfield_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, column_view_set_id);
  layout->AddView(new views::Label(l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_GATEWAY)));
  gateway_textfield_ = new views::Textfield(views::Textfield::STYLE_DEFAULT);
  gateway_textfield_->SetEnabled(false);
  layout->AddView(gateway_textfield_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(0, column_view_set_id);
  layout->AddView(new views::Label(l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_DNSSERVER)));
  dnsserver_textfield_ = new views::Textfield(views::Textfield::STYLE_DEFAULT);
  dnsserver_textfield_->SetEnabled(false);
  layout->AddView(dnsserver_textfield_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
}

}  // namespace chromeos
