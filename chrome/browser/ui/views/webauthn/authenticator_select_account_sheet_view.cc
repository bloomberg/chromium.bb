// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "chrome/browser/ui/views/webauthn/authenticator_select_account_sheet_view.h"
#include "chrome/grit/generated_resources.h"
#include "device/fido/authenticator_get_assertion_response.h"
#include "ui/base/models/table_model.h"
#include "ui/base/models/table_model_observer.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/controls/table/table_view.h"

AuthenticatorSelectAccountSheetView::AuthenticatorSelectAccountSheetView(
    std::unique_ptr<AuthenticatorSelectAccountSheetModel> sheet_model)
    : AuthenticatorRequestSheetView(std::move(sheet_model)) {}

AuthenticatorSelectAccountSheetView::~AuthenticatorSelectAccountSheetView() {
  if (table_) {
    table_->SetModel(nullptr);
    table_->set_observer(nullptr);
  }
}

std::unique_ptr<views::View>
AuthenticatorSelectAccountSheetView::BuildStepSpecificContent() {
  std::vector<ui::TableColumn> columns;
  columns.emplace_back(IDS_WEBAUTHN_ACCOUNT_COLUMN, ui::TableColumn::LEFT, -1,
                       0);
  columns.emplace_back(IDS_WEBAUTHN_NAME_COLUMN, ui::TableColumn::LEFT, -1, 0);
  auto* sheet_model =
      static_cast<AuthenticatorSelectAccountSheetModel*>(model());
  auto table = std::make_unique<views::TableView>(
      sheet_model, columns, views::TEXT_ONLY, true /* single_selection */);
  table_ = table.get();
  table_->set_observer(this);
  auto scroll_view =
      views::TableView::CreateScrollViewWithTable(std::move(table));

  // The table is packed into a BoxLayout, which will allocate the minimum
  // vertical size to each element. However, the default minimum size of the
  // table's ScrollView is zero, so a vertical size must be set here to prevent
  // the table disappearing.
  scroll_view->SetPreferredSize(gfx::Size(
      0, std::min(static_cast<size_t>(200u),
                  50 * (1 + sheet_model->dialog_model()->responses().size()))));
  return std::move(scroll_view);
}

void AuthenticatorSelectAccountSheetView::OnSelectionChanged() {
  auto* sheet_model =
      static_cast<AuthenticatorSelectAccountSheetModel*>(model());
  sheet_model->SetCurrentSelection(table_->FirstSelectedRow());
}

void AuthenticatorSelectAccountSheetView::OnDoubleClick() {
  auto* sheet_model =
      static_cast<AuthenticatorSelectAccountSheetModel*>(model());
  sheet_model->SetCurrentSelection(table_->FirstSelectedRow());
  sheet_model->OnAccept();
}
