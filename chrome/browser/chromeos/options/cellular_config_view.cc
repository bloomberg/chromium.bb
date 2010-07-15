// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/cellular_config_view.h"

#include "app/l10n_util.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/options/network_config_view.h"
#include "grit/generated_resources.h"
#include "views/controls/label.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"

namespace chromeos {

CellularConfigView::CellularConfigView(NetworkConfigView* parent,
                                       const CellularNetwork& cellular)
    : parent_(parent),
      cellular_(cellular),
      autoconnect_checkbox_(NULL) {
  Init();
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

  int column_view_set_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(column_view_set_id);
  column_set->AddColumn(views::GridLayout::LEADING, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);
  column_set->AddColumn(views::GridLayout::FILL, views::GridLayout::FILL, 1,
                        views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, column_view_set_id);
  layout->AddView(new views::Label(l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_NETWORK_ID)));
  views::Label* label = new views::Label(ASCIIToWide(cellular_.name()));
  label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->AddView(label);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  // Autoconnect checkbox
  autoconnect_checkbox_ = new views::Checkbox(
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_INTERNET_OPTIONS_AUTO_CONNECT));
  autoconnect_checkbox_->SetChecked(cellular_.auto_connect());
  layout->StartRow(0, column_view_set_id);
  layout->AddView(autoconnect_checkbox_, 2, 1);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
}

}  // namespace chromeos
