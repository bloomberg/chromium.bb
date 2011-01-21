// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/options/exceptions_view.h"

#include <algorithm>
#include <vector>

#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/views/options/content_exceptions_table_view.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "gfx/rect.h"
#include "ui/base/l10n/l10n_util.h"
#include "views/controls/button/native_button.h"
#include "views/controls/label.h"
#include "views/controls/table/table_view.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

static const int kExceptionsViewInsetSize = 5;
static ExceptionsView* instances[CONTENT_SETTINGS_NUM_TYPES] = { NULL };

// static
void ExceptionsView::ShowExceptionsWindow(
    gfx::NativeWindow parent,
    HostContentSettingsMap* map,
    HostContentSettingsMap* off_the_record_map,
    ContentSettingsType content_type) {
  if (!instances[content_type]) {
    instances[content_type] =
        new ExceptionsView(map, off_the_record_map, content_type);
    views::Window::CreateChromeWindow(parent, gfx::Rect(),
                                      instances[content_type]);
  }

  // This will show invisible windows and bring visible windows to the front.
  instances[content_type]->window()->Show();
}

ExceptionsView::~ExceptionsView() {
  instances[model_.content_type()] = NULL;
  table_->SetModel(NULL);
}

void ExceptionsView::OnSelectionChanged() {
  UpdateButtonState();
}

void ExceptionsView::OnDoubleClick() {
  if (table_->SelectedRowCount() == 1)
    Edit();
}

void ExceptionsView::OnTableViewDelete(views::TableView* table_view) {
  Remove();
}

void ExceptionsView::ButtonPressed(views::Button* sender,
                                   const views::Event& event) {
  switch (sender->tag()) {
    case IDS_EXCEPTIONS_ADD_BUTTON:
      Add();
      break;
    case IDS_EXCEPTIONS_EDIT_BUTTON:
      Edit();
      break;
    case IDS_EXCEPTIONS_REMOVEALL_BUTTON:
      RemoveAll();
      break;
    case IDS_EXCEPTIONS_REMOVE_BUTTON:
      Remove();
      break;
    default:
      NOTREACHED();
  }
}

void ExceptionsView::Layout() {
  views::NativeButton* buttons[] = { add_button_, edit_button_,
                                     remove_button_, remove_all_button_ };

  // The buttons are placed in the parent, but we need to lay them out.
  int max_y = GetParent()->GetLocalBounds(false).bottom() - kButtonVEdgeMargin;
  int x = kPanelHorizMargin;

  for (size_t i = 0; i < arraysize(buttons); ++i) {
    gfx::Size pref = buttons[i]->GetPreferredSize();
    buttons[i]->SetBounds(x, max_y - pref.height(), pref.width(),
                          pref.height());
    x += pref.width() + kRelatedControlHorizontalSpacing;
  }

  // Lay out the rest of this view.
  View::Layout();
}

gfx::Size ExceptionsView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_CONTENT_EXCEPTION_DIALOG_WIDTH_CHARS,
      IDS_CONTENT_EXCEPTION_DIALOG_HEIGHT_LINES));
}

void ExceptionsView::ViewHierarchyChanged(bool is_add,
                                          views::View* parent,
                                          views::View* child) {
  if (is_add && child == this)
    Init();
}

std::wstring ExceptionsView::GetWindowTitle() const {
  switch (model_.content_type()) {
    case CONTENT_SETTINGS_TYPE_COOKIES:
      return UTF16ToWide(l10n_util::GetStringUTF16(IDS_COOKIE_EXCEPTION_TITLE));
    case CONTENT_SETTINGS_TYPE_IMAGES:
      return UTF16ToWide(l10n_util::GetStringUTF16(IDS_IMAGES_EXCEPTION_TITLE));
    case CONTENT_SETTINGS_TYPE_JAVASCRIPT:
      return UTF16ToWide(l10n_util::GetStringUTF16(IDS_JS_EXCEPTION_TITLE));
    case CONTENT_SETTINGS_TYPE_PLUGINS:
      return UTF16ToWide(
          l10n_util::GetStringUTF16(IDS_PLUGINS_EXCEPTION_TITLE));
    case CONTENT_SETTINGS_TYPE_POPUPS:
      return UTF16ToWide(l10n_util::GetStringUTF16(IDS_POPUP_EXCEPTION_TITLE));
    default:
      NOTREACHED();
  }
  return std::wstring();
}

void ExceptionsView::AcceptExceptionEdit(
    const ContentSettingsPattern& pattern,
    ContentSetting setting,
    bool is_off_the_record,
    int index,
    bool is_new) {
  DCHECK(!is_off_the_record || allow_off_the_record_);

  if (!is_new)
    model_.RemoveException(index);
  model_.AddException(pattern, setting, is_off_the_record);

  int new_index = model_.IndexOfExceptionByPattern(pattern, is_off_the_record);
  DCHECK(new_index != -1);
  table_->Select(new_index);
}

ExceptionsView::ExceptionsView(HostContentSettingsMap* map,
                               HostContentSettingsMap* off_the_record_map,
                               ContentSettingsType type)
    : model_(map, off_the_record_map, type),
      allow_off_the_record_(off_the_record_map != NULL),
      table_(NULL),
      add_button_(NULL),
      edit_button_(NULL),
      remove_button_(NULL),
      remove_all_button_(NULL) {
}

void ExceptionsView::Init() {
  if (table_)
    return;  // We've already Init'd.

  using views::GridLayout;

  std::vector<ui::TableColumn> columns;
  columns.push_back(
      ui::TableColumn(IDS_EXCEPTIONS_PATTERN_HEADER, ui::TableColumn::LEFT, -1,
                      .75));
  columns.back().sortable = true;
  columns.push_back(
      ui::TableColumn(IDS_EXCEPTIONS_ACTION_HEADER, ui::TableColumn::LEFT, -1,
                      .25));
  columns.back().sortable = true;
  table_ = new ContentExceptionsTableView(&model_, columns);
  views::TableView::SortDescriptors sort;
  sort.push_back(
      views::TableView::SortDescriptor(IDS_EXCEPTIONS_PATTERN_HEADER, true));
  table_->SetSortDescriptors(sort);
  table_->SetObserver(this);

  add_button_ = new views::NativeButton(
      this, UTF16ToWide(l10n_util::GetStringUTF16(IDS_EXCEPTIONS_ADD_BUTTON)));
  add_button_->set_tag(IDS_EXCEPTIONS_ADD_BUTTON);
  edit_button_ = new views::NativeButton(
      this, UTF16ToWide(l10n_util::GetStringUTF16(IDS_EXCEPTIONS_EDIT_BUTTON)));
  edit_button_->set_tag(IDS_EXCEPTIONS_EDIT_BUTTON);
  remove_button_ = new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_EXCEPTIONS_REMOVE_BUTTON)));
  remove_button_->set_tag(IDS_EXCEPTIONS_REMOVE_BUTTON);
  remove_all_button_ = new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(IDS_EXCEPTIONS_REMOVEALL_BUTTON)));
  remove_all_button_->set_tag(IDS_EXCEPTIONS_REMOVEALL_BUTTON);

  View* parent = GetParent();
  parent->AddChildView(add_button_);
  parent->AddChildView(edit_button_);
  parent->AddChildView(remove_button_);
  parent->AddChildView(remove_all_button_);

  GridLayout* layout = new GridLayout(this);
  layout->SetInsets(kExceptionsViewInsetSize, kExceptionsViewInsetSize,
                    kExceptionsViewInsetSize, kExceptionsViewInsetSize);
  SetLayoutManager(layout);

  const int single_column_layout_id = 0;
  views::ColumnSet* column_set = layout->AddColumnSet(single_column_layout_id);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  layout->StartRow(1, single_column_layout_id);
  layout->AddView(table_);
  layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);

  if (allow_off_the_record_) {
    views::Label* label = new views::Label(
        UTF16ToWide(l10n_util::GetStringUTF16(IDS_EXCEPTIONS_OTR_IN_ITALICS)));
    label->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
    layout->StartRow(0, single_column_layout_id);
    layout->AddView(label, 1, 1, GridLayout::LEADING, GridLayout::CENTER);
    layout->AddPaddingRow(0, kRelatedControlVerticalSpacing);
  }

  UpdateButtonState();
}

void ExceptionsView::UpdateButtonState() {
  int selected_row_count = table_->SelectedRowCount();
  // TODO: 34177, support editing of more than one entry at a time.
  edit_button_->SetEnabled(selected_row_count == 1);
  remove_button_->SetEnabled(selected_row_count >= 1);
  remove_all_button_->SetEnabled(model_.RowCount() > 0);
}

void ExceptionsView::Add() {
  ExceptionEditorView* view =
      new ExceptionEditorView(this, &model_, allow_off_the_record_, -1,
                              ContentSettingsPattern(),
                              CONTENT_SETTING_BLOCK, false);
  view->Show(window()->GetNativeWindow());

  UpdateButtonState();
}

void ExceptionsView::Edit() {
  DCHECK(table_->FirstSelectedRow() != -1);
  int index = table_->FirstSelectedRow();
  const HostContentSettingsMap::PatternSettingPair& entry =
      model_.entry_at(index);
  ExceptionEditorView* view =
      new ExceptionEditorView(this, &model_, allow_off_the_record_, index,
                              entry.first, entry.second,
                              model_.entry_is_off_the_record(index));
  view->Show(window()->GetNativeWindow());
}

void ExceptionsView::Remove() {
  std::vector<int> indices;
  for (views::TableView::iterator i = table_->SelectionBegin();
       i != table_->SelectionEnd(); ++i) {
    indices.push_back(*i);
  }
  std::sort(indices.begin(), indices.end());
  for (std::vector<int>::reverse_iterator i = indices.rbegin();
       i != indices.rend(); ++i) {
    model_.RemoveException(*i);
  }

  UpdateButtonState();
}

void ExceptionsView::RemoveAll() {
  model_.RemoveAll();

  UpdateButtonState();
}
