// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/clear_data_view.h"

#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/ui/views/clear_browsing_data.h"
#include "chrome/browser/ui/views/clear_server_data.h"
#include "gfx/insets.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "net/url_request/url_request_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/background.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/label.h"
#include "views/controls/separator.h"
#include "views/controls/throbber.h"
#include "views/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/widget/widget.h"
#include "views/window/dialog_client_view.h"
#include "views/window/window.h"

using views::GridLayout;

// The combo box is vertically aligned to the 'time-period' label, which makes
// the combo box look a little too close to the check box above it when we use
// standard layout to separate them. We therefore add a little extra margin to
// the label, giving it a little breathing space.
static const int kExtraMarginForTimePeriodLabel = 3;

////////////////////////////////////////////////////////////////////////////////
// ClearDataView, public:

ClearDataView::ClearDataView(Profile* profile)
    : profile_(profile),
      clear_server_data_tab_(NULL),
      clear_browsing_data_tab_(NULL),
      clearing_data_(false) {
  DCHECK(profile);
  Init();
}

void ClearDataView::Init() {
  tabs_ = new views::TabbedPane;

  tabs_->SetAccessibleName(l10n_util::GetStringFUTF16(
      IDS_OPTIONS_DIALOG_TITLE,
      l10n_util::GetStringUTF16(IDS_OPTIONS_DIALOG_TITLE)));
  AddChildView(tabs_);

  int tab_index = 0;
  clear_browsing_data_tab_ = new ClearBrowsingDataView2(profile_, this);
  tabs_->AddTabAtIndex(
      tab_index++,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_CLEAR_CHROME_DATA_TAB_LABEL)),
      clear_browsing_data_tab_, false);
  clear_server_data_tab_ = new ClearServerDataView(profile_, this);
  tabs_->AddTabAtIndex(
      tab_index++,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_CLEAR_OTHER_DATA_TAB_LABEL)),
      clear_server_data_tab_, false);

  tabs_->SelectTabAt(static_cast<int>(0));
}

void ClearDataView::StartClearingBrowsingData() {
  // Only one clear can happen at a time
  clear_server_data_tab_->SetAllowClear(false);
  clearing_data_ = true;
  window()->EnableClose(false);
  GetDialogClientView()->UpdateDialogButtons();
}

void ClearDataView::StopClearingBrowsingData() {
  window()->Close();
}

void ClearDataView::StartClearingServerData() {
  // Only one clear can happen at a time
  clear_browsing_data_tab_->SetAllowClear(false);
  clearing_data_ = true;
  window()->EnableClose(false);
  GetDialogClientView()->UpdateDialogButtons();
}

void ClearDataView::SucceededClearingServerData() {
  window()->Close();
}

void ClearDataView::FailedClearingServerData() {
  clear_browsing_data_tab_->SetAllowClear(true);
  clearing_data_ = false;
  window()->EnableClose(false);
  GetDialogClientView()->UpdateDialogButtons();
}

////////////////////////////////////////////////////////////////////////////////
// ClearDataView, views::View implementation:

gfx::Size ClearDataView::GetPreferredSize() {
  gfx::Size size(tabs_->GetPreferredSize());
  size.Enlarge(2 * kDialogPadding, 2 * kDialogPadding);
  return size;
}

void ClearDataView::Layout() {
  tabs_->SetBounds(kDialogPadding, kDialogPadding,
                   width() - (2 * kDialogPadding),
                   height() - (2 * kDialogPadding));
}

////////////////////////////////////////////////////////////////////////////////
// ClearDataView, views::DialogDelegate implementation:

int ClearDataView::GetDefaultDialogButton() const {
  return MessageBoxFlags::DIALOGBUTTON_NONE;
}

std::wstring ClearDataView::GetDialogButtonLabel(
    MessageBoxFlags::DialogButton button) const {
  DCHECK(button == MessageBoxFlags::DIALOGBUTTON_CANCEL);
  return UTF16ToWide(l10n_util::GetStringUTF16(IDS_CANCEL));
}

int ClearDataView::GetDialogButtons() const {
  return MessageBoxFlags::DIALOGBUTTON_CANCEL;
}


bool ClearDataView::IsDialogButtonEnabled(
    MessageBoxFlags::DialogButton button) const {

  return !clearing_data_;
}

bool ClearDataView::CanResize() const {
  return false;
}

bool ClearDataView::CanMaximize() const {
  return false;
}

bool ClearDataView::IsAlwaysOnTop() const {
  return false;
}

bool ClearDataView::HasAlwaysOnTopMenu() const {
  return false;
}

bool ClearDataView::IsModal() const {
  return true;
}

std::wstring ClearDataView::GetWindowTitle() const {
  return UTF16ToWide(l10n_util::GetStringUTF16(IDS_CLEAR_BROWSING_DATA_TITLE));
}

views::View* ClearDataView::GetContentsView() {
  return this;
}

views::View* ClearDataView::GetInitiallyFocusedView() {
  return GetDialogClientView()->cancel_button();
}
