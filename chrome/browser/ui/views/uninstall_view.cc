// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/uninstall_view.h"

#include "base/message_loop.h"
#include "base/process_util.h"
#include "base/string16.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/shell_integration.h"
#include "chrome/browser/ui/uninstall_browser_prompt.h"
#include "chrome/common/chrome_result_codes.h"
#include "chrome/installer/util/browser_distribution.h"
#include "chrome/installer/util/shell_util.h"
#include "grit/chromium_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/controls/button/checkbox.h"
#include "ui/views/controls/combobox/combobox.h"
#include "ui/views/controls/label.h"
#include "ui/views/focus/accelerator_handler.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"

UninstallView::UninstallView(int* user_selection)
    : confirm_label_(NULL),
      delete_profile_(NULL),
      change_default_browser_(NULL),
      browsers_combo_(NULL),
      browsers_(NULL),
      user_selection_(*user_selection) {
  SetupControls();
}

UninstallView::~UninstallView() {
  // Exit the message loop we were started with so that uninstall can continue.
  MessageLoop::current()->Quit();
}

void UninstallView::SetupControls() {
  using views::ColumnSet;
  using views::GridLayout;

  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  // Message to confirm uninstallation.
  int column_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(column_set_id);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, column_set_id);
  confirm_label_ = new views::Label(
      l10n_util::GetStringUTF16(IDS_UNINSTALL_VERIFY));
  confirm_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  layout->AddView(confirm_label_);

  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  // The "delete profile" check box.
  ++column_set_id;
  column_set = layout->AddColumnSet(column_set_id);
  column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  layout->StartRow(0, column_set_id);
  delete_profile_ = new views::Checkbox(
      l10n_util::GetStringUTF16(IDS_UNINSTALL_DELETE_PROFILE));
  layout->AddView(delete_profile_);

  // Set default browser combo box
  BrowserDistribution* dist = BrowserDistribution::GetDistribution();
  if (dist->CanSetAsDefault() &&
      ShellIntegration::IsDefaultBrowser()) {
    browsers_.reset(new BrowsersMap());
    ShellUtil::GetRegisteredBrowsers(dist, browsers_.get());
    if (!browsers_->empty()) {
      layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);

      ++column_set_id;
      column_set = layout->AddColumnSet(column_set_id);
      column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
      column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                            GridLayout::USE_PREF, 0, 0);
      column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
      column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                            GridLayout::USE_PREF, 0, 0);
      layout->StartRow(0, column_set_id);
      change_default_browser_ = new views::Checkbox(
          l10n_util::GetStringUTF16(IDS_UNINSTALL_SET_DEFAULT_BROWSER));
      change_default_browser_->set_listener(this);
      layout->AddView(change_default_browser_);
      browsers_combo_ = new views::Combobox(this);
      layout->AddView(browsers_combo_);
      browsers_combo_->SetEnabled(false);
    }
  }

  layout->AddPaddingRow(0, views::kRelatedControlSmallVerticalSpacing);
}

bool UninstallView::Accept() {
  user_selection_ = content::RESULT_CODE_NORMAL_EXIT;
  if (delete_profile_->checked())
    user_selection_ = chrome::RESULT_CODE_UNINSTALL_DELETE_PROFILE;
  if (change_default_browser_ && change_default_browser_->checked()) {
    BrowsersMap::const_iterator it = browsers_->begin();
    std::advance(it, browsers_combo_->selected_index());
    base::LaunchOptions options;
    options.start_hidden = true;
    base::LaunchProcess((*it).second, options, NULL);
  }
  return true;
}

bool UninstallView::Cancel() {
  user_selection_ = chrome::RESULT_CODE_UNINSTALL_USER_CANCEL;
  return true;
}

string16 UninstallView::GetDialogButtonLabel(ui::DialogButton button) const {
  // We only want to give custom name to OK button - 'Uninstall'. Cancel
  // button remains same.
  if (button == ui::DIALOG_BUTTON_OK)
    return l10n_util::GetStringUTF16(IDS_UNINSTALL_BUTTON_TEXT);
  return string16();
}

void UninstallView::ButtonPressed(views::Button* sender,
                                  const views::Event& event) {
  if (change_default_browser_ == sender) {
    // Disable the browsers combobox if the user unchecks the checkbox.
    DCHECK(browsers_combo_);
    browsers_combo_->SetEnabled(change_default_browser_->checked());
  }
}

string16 UninstallView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_UNINSTALL_CHROME);
}

views::View* UninstallView::GetContentsView() {
  return this;
}

int UninstallView::GetItemCount() const {
  DCHECK(!browsers_->empty());
  return browsers_->size();
}

string16 UninstallView::GetItemAt(int index) {
  DCHECK_LT(index, static_cast<int>(browsers_->size()));
  BrowsersMap::const_iterator it = browsers_->begin();
  std::advance(it, index);
  return WideToUTF16Hack((*it).first);
}

namespace browser {

int ShowUninstallBrowserPrompt() {
  int result = content::RESULT_CODE_NORMAL_EXIT;
  views::Widget::CreateWindow(new UninstallView(&result))->Show();
  views::AcceleratorHandler accelerator_handler;
  MessageLoopForUI::current()->RunWithDispatcher(&accelerator_handler);
  return result;
}

}  // namespace browser
