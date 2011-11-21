// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/importer/import_progress_dialog_view.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/importer/importer_host.h"
#include "chrome/browser/importer/importer_observer.h"
#include "chrome/browser/importer/importer_progress_dialog.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "views/controls/label.h"
#include "views/controls/throbber.h"

ImportProgressDialogView::ImportProgressDialogView(
    HWND parent_window,
    uint16 items,
    ImporterHost* importer_host,
    ImporterObserver* importer_observer,
    const string16& importer_name,
    bool bookmarks_import)
    : state_bookmarks_(new views::CheckmarkThrobber),
      state_searches_(new views::CheckmarkThrobber),
      state_passwords_(new views::CheckmarkThrobber),
      state_history_(new views::CheckmarkThrobber),
      state_cookies_(new views::CheckmarkThrobber),
      label_bookmarks_(new views::Label(
          l10n_util::GetStringUTF16(IDS_IMPORT_PROGRESS_STATUS_BOOKMARKS))),
      label_searches_(new views::Label(
          l10n_util::GetStringUTF16(IDS_IMPORT_PROGRESS_STATUS_SEARCH))),
      label_passwords_(new views::Label(
          l10n_util::GetStringUTF16(IDS_IMPORT_PROGRESS_STATUS_PASSWORDS))),
      label_history_(new views::Label(
          l10n_util::GetStringUTF16(IDS_IMPORT_PROGRESS_STATUS_HISTORY))),
      label_cookies_(new views::Label(
          l10n_util::GetStringUTF16(IDS_IMPORT_PROGRESS_STATUS_COOKIES))),
      parent_window_(parent_window),
      items_(items),
      importer_host_(importer_host),
      importer_observer_(importer_observer),
      importing_(true),
      bookmarks_import_(bookmarks_import) {
  std::wstring info_text = bookmarks_import ?
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_IMPORT_BOOKMARKS)) :
      UTF16ToWide(l10n_util::GetStringFUTF16(
          IDS_IMPORT_PROGRESS_INFO, importer_name));
  label_info_ = new views::Label(info_text);
  importer_host_->SetObserver(this);
  label_info_->SetMultiLine(true);
  label_info_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label_bookmarks_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label_searches_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label_passwords_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label_history_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  label_cookies_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);

  // These are scoped pointers, so we don't need the parent to delete them.
  state_bookmarks_->set_parent_owned(false);
  state_searches_->set_parent_owned(false);
  state_passwords_->set_parent_owned(false);
  state_history_->set_parent_owned(false);
  state_cookies_->set_parent_owned(false);
  label_bookmarks_->set_parent_owned(false);
  label_searches_->set_parent_owned(false);
  label_passwords_->set_parent_owned(false);
  label_history_->set_parent_owned(false);
  label_cookies_->set_parent_owned(false);
}

ImportProgressDialogView::~ImportProgressDialogView() {
  RemoveChildView(state_bookmarks_.get());
  RemoveChildView(state_searches_.get());
  RemoveChildView(state_passwords_.get());
  RemoveChildView(state_history_.get());
  RemoveChildView(state_cookies_.get());
  RemoveChildView(label_bookmarks_.get());
  RemoveChildView(label_searches_.get());
  RemoveChildView(label_passwords_.get());
  RemoveChildView(label_history_.get());
  RemoveChildView(label_cookies_.get());

  if (importing_) {
    // We're being deleted while importing, clean up state so that the importer
    // doesn't have a reference to us and cancel the import. We can get here
    // if our parent window is closed, which closes our window and deletes us.
    importing_ = false;
    importer_host_->SetObserver(NULL);
    importer_host_->Cancel();
    if (importer_observer_)
      importer_observer_->ImportCompleted();
  }
}

gfx::Size ImportProgressDialogView::GetPreferredSize() {
  return gfx::Size(views::Widget::GetLocalizedContentsSize(
      IDS_IMPORTPROGRESS_DIALOG_WIDTH_CHARS,
      IDS_IMPORTPROGRESS_DIALOG_HEIGHT_LINES));
}

void ImportProgressDialogView::ViewHierarchyChanged(bool is_add,
                                                    views::View* parent,
                                                    views::View* child) {
  if (is_add && child == this)
    InitControlLayout();
}

int ImportProgressDialogView::GetDialogButtons() const {
  return ui::DIALOG_BUTTON_CANCEL;
}

string16 ImportProgressDialogView::GetDialogButtonLabel(
    ui::DialogButton button) const {
  DCHECK_EQ(button, ui::DIALOG_BUTTON_CANCEL);
  return l10n_util::GetStringUTF16(IDS_IMPORT_PROGRESS_STATUS_CANCEL);
}

bool ImportProgressDialogView::IsModal() const {
  return parent_window_ != NULL;
}

string16 ImportProgressDialogView::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(IDS_IMPORT_PROGRESS_TITLE);
}

bool ImportProgressDialogView::Cancel() {
  // When the user cancels the import, we need to tell the importer_host to stop
  // importing and return false so that the window lives long enough to receive
  // ImportEnded, which will close the window. Closing the window results in
  // another call to this function and at that point we must return true to
  // allow the window to close.
  if (!importing_)
    return true;  // We have received ImportEnded, so we can close.

  // Cancel the import and wait for further instructions.
  importer_host_->Cancel();
  return false;
}

views::View* ImportProgressDialogView::GetContentsView() {
  return this;
}

void ImportProgressDialogView::InitControlLayout() {
  using views::GridLayout;
  using views::ColumnSet;

  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  gfx::Size ps = state_history_->GetPreferredSize();

  const int single_column_view_set_id = 0;
  ColumnSet* column_set = layout->AddColumnSet(single_column_view_set_id);
  if (bookmarks_import_) {
    column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                          GridLayout::FIXED, ps.width(), 0);
    column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  }
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  const int double_column_view_set_id = 1;
  column_set = layout->AddColumnSet(double_column_view_set_id);
  column_set->AddPaddingColumn(
      0, views::kUnrelatedControlLargeHorizontalSpacing);
  column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                        GridLayout::FIXED, ps.width(), 0);
  column_set->AddPaddingColumn(0, views::kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(
      0, views::kUnrelatedControlLargeHorizontalSpacing);

  layout->StartRow(0, single_column_view_set_id);
  if (bookmarks_import_)
    layout->AddView(state_bookmarks_.get());
  layout->AddView(label_info_);
  layout->AddPaddingRow(0, views::kUnrelatedControlVerticalSpacing);

  if (items_ & importer::HISTORY) {
    layout->StartRow(0, double_column_view_set_id);
    layout->AddView(state_history_.get());
    layout->AddView(label_history_.get());
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  }
  if (items_ & importer::FAVORITES && !bookmarks_import_) {
    layout->StartRow(0, double_column_view_set_id);
    layout->AddView(state_bookmarks_.get());
    layout->AddView(label_bookmarks_.get());
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  }
  if (items_ & importer::SEARCH_ENGINES) {
    layout->StartRow(0, double_column_view_set_id);
    layout->AddView(state_searches_.get());
    layout->AddView(label_searches_.get());
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  }
  if (items_ & importer::PASSWORDS) {
    layout->StartRow(0, double_column_view_set_id);
    layout->AddView(state_passwords_.get());
    layout->AddView(label_passwords_.get());
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  }
  if (items_ & importer::COOKIES) {
    layout->StartRow(0, double_column_view_set_id);
    layout->AddView(state_cookies_.get());
    layout->AddView(label_cookies_.get());
    layout->AddPaddingRow(0, views::kRelatedControlVerticalSpacing);
  }
}

void ImportProgressDialogView::ImportStarted() {
  importing_ = true;
}

void ImportProgressDialogView::ImportItemStarted(importer::ImportItem item) {
  DCHECK(items_ & item);
  switch (item) {
    case importer::FAVORITES:
      state_bookmarks_->Start();
      break;
    case importer::SEARCH_ENGINES:
      state_searches_->Start();
      break;
    case importer::PASSWORDS:
      state_passwords_->Start();
      break;
    case importer::HISTORY:
      state_history_->Start();
      break;
    case importer::COOKIES:
      state_cookies_->Start();
      break;
  }
}

void ImportProgressDialogView::ImportItemEnded(importer::ImportItem item) {
  DCHECK(items_ & item);
  switch (item) {
    case importer::FAVORITES:
      state_bookmarks_->Stop();
      state_bookmarks_->SetChecked(true);
      break;
    case importer::SEARCH_ENGINES:
      state_searches_->Stop();
      state_searches_->SetChecked(true);
      break;
    case importer::PASSWORDS:
      state_passwords_->Stop();
      state_passwords_->SetChecked(true);
      break;
    case importer::HISTORY:
      state_history_->Stop();
      state_history_->SetChecked(true);
      break;
    case importer::COOKIES:
      state_cookies_->Stop();
      state_cookies_->SetChecked(true);
      break;
  }
}

void ImportProgressDialogView::ImportEnded() {
  // This can happen because:
  // - the import completed successfully.
  // - the import was canceled by the user.
  // - the user chose to skip the import because they didn't want to shut down
  //   Firefox.
  // In every case, we need to close the UI now.
  importing_ = false;
  importer_host_->SetObserver(NULL);
  GetWidget()->Close();
  if (importer_observer_)
    importer_observer_->ImportCompleted();
}

namespace importer {

void ShowImportProgressDialog(HWND parent_window,
                              uint16 items,
                              ImporterHost* importer_host,
                              ImporterObserver* importer_observer,
                              const SourceProfile& source_profile,
                              Profile* target_profile,
                              bool first_run) {
  DCHECK_NE(items, 0u);
  ImportProgressDialogView* progress_view = new ImportProgressDialogView(
      parent_window,
      items,
      importer_host,
      importer_observer,
      source_profile.importer_name,
      source_profile.importer_type == importer::TYPE_BOOKMARKS_FILE);

  views::Widget* window =
      views::Widget::CreateWindowWithParent(progress_view, parent_window);

  if (!importer_host->is_headless() && !first_run)
    window->Show();

  importer_host->StartImportSettings(
      source_profile, target_profile, items, new ProfileWriter(target_profile),
      first_run);
}

}  // namespace importer
