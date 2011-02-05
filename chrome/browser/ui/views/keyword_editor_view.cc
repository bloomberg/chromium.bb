// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/keyword_editor_view.h"

#include <string>
#include <vector>

#include "base/stl_util-inl.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/search_engines/template_url.h"
#include "chrome/browser/search_engines/template_url_model.h"
#include "chrome/browser/search_engines/template_url_table_model.h"
#include "chrome/browser/ui/views/browser_dialogs.h"
#include "chrome/browser/ui/views/first_run_search_engine_view.h"
#include "chrome/common/pref_names.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/gfx/point.h"
#include "views/background.h"
#include "views/controls/button/native_button.h"
#include "views/controls/table/table_view.h"
#include "views/controls/textfield/textfield.h"
#include "views/layout/grid_layout.h"
#include "views/layout/layout_constants.h"
#include "views/widget/widget.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

using views::GridLayout;
using views::NativeButton;

namespace browser {

// Declared in browser_dialogs.h so others don't have to depend on our header.
void ShowKeywordEditorView(Profile* profile) {
  KeywordEditorView::Show(profile);
}

}  // namespace browser


// KeywordEditorView ----------------------------------------------------------

// If non-null, there is an open editor and this is the window it is contained
// in.
static views::Window* open_window = NULL;

// static
// The typical case for showing a KeywordEditorView does not involve an
// observer, so use this function signature generally.
void KeywordEditorView::Show(Profile* profile) {
  KeywordEditorView::ShowAndObserve(profile, NULL);
}

// static
void KeywordEditorView::ShowAndObserve(Profile* profile,
    SearchEngineSelectionObserver* observer) {
  // If this panel is opened from an Incognito window, closing that window can
  // leave this with a stale pointer. Use the original profile instead.
  // See http://crbug.com/23359.
  profile = profile ->GetOriginalProfile();
  if (!profile->GetTemplateURLModel())
    return;

  if (open_window != NULL)
    open_window->Close();
  DCHECK(!open_window);

  // Both of these will be deleted when the dialog closes.
  KeywordEditorView* keyword_editor = new KeywordEditorView(profile, observer);

  // Initialize the UI. By passing in an empty rect KeywordEditorView is
  // queried for its preferred size.
  open_window = views::Window::CreateChromeWindow(NULL, gfx::Rect(),
                                                  keyword_editor);

  open_window->Show();
}

KeywordEditorView::KeywordEditorView(Profile* profile,
                                     SearchEngineSelectionObserver* observer)
    : profile_(profile),
      observer_(observer),
      controller_(new KeywordEditorController(profile)),
      default_chosen_(false) {
  DCHECK(controller_->url_model());
  controller_->url_model()->AddObserver(this);
  Init();
}

KeywordEditorView::~KeywordEditorView() {
  table_view_->SetModel(NULL);
  controller_->url_model()->RemoveObserver(this);
}

void KeywordEditorView::OnEditedKeyword(const TemplateURL* template_url,
                                        const string16& title,
                                        const string16& keyword,
                                        const std::string& url) {
  if (template_url) {
    controller_->ModifyTemplateURL(template_url, title, keyword, url);

    // Force the make default button to update.
    OnSelectionChanged();
  } else {
    table_view_->Select(controller_->AddTemplateURL(title, keyword, url));
  }
}


gfx::Size KeywordEditorView::GetPreferredSize() {
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_SEARCHENGINES_DIALOG_WIDTH_CHARS,
      IDS_SEARCHENGINES_DIALOG_HEIGHT_LINES));
}

bool KeywordEditorView::CanResize() const {
  return true;
}

std::wstring KeywordEditorView::GetWindowTitle() const {
  return UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_EDITOR_WINDOW_TITLE));
}

std::wstring KeywordEditorView::GetWindowName() const {
  return UTF8ToWide(prefs::kKeywordEditorWindowPlacement);
}

int KeywordEditorView::GetDialogButtons() const {
  return MessageBoxFlags::DIALOGBUTTON_CANCEL;
}

bool KeywordEditorView::Accept() {
  open_window = NULL;
  return true;
}

bool KeywordEditorView::Cancel() {
  open_window = NULL;
  return true;
}

views::View* KeywordEditorView::GetContentsView() {
  return this;
}

void KeywordEditorView::Init() {
  std::vector<ui::TableColumn> columns;
  columns.push_back(
      ui::TableColumn(IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_COLUMN,
                      ui::TableColumn::LEFT, -1, .75));
  columns.back().sortable = true;
  columns.push_back(
      ui::TableColumn(IDS_SEARCH_ENGINES_EDITOR_KEYWORD_COLUMN,
                      ui::TableColumn::LEFT, -1, .25));
  columns.back().sortable = true;
  table_view_ = new views::TableView(controller_->table_model(), columns,
      views::ICON_AND_TEXT, false, true, true);
  table_view_->SetObserver(this);

  add_button_ = new views::NativeButton(this, UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_EDITOR_NEW_BUTTON)));
  add_button_->SetEnabled(controller_->loaded());
  add_button_->AddAccelerator(
      views::Accelerator(ui::VKEY_A, false, false, true));
  add_button_->SetAccessibleKeyboardShortcut(L"A");

  edit_button_ = new views::NativeButton(this, UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_EDITOR_EDIT_BUTTON)));
  edit_button_->SetEnabled(false);

  remove_button_ = new views::NativeButton(this, UTF16ToWide(
      l10n_util::GetStringUTF16(IDS_SEARCH_ENGINES_EDITOR_REMOVE_BUTTON)));
  remove_button_->SetEnabled(false);
  remove_button_->AddAccelerator(
      views::Accelerator(ui::VKEY_R, false, false, true));
  remove_button_->SetAccessibleKeyboardShortcut(L"R");

  make_default_button_ = new views::NativeButton(
      this,
      UTF16ToWide(l10n_util::GetStringUTF16(
          IDS_SEARCH_ENGINES_EDITOR_MAKE_DEFAULT_BUTTON)));
  make_default_button_->SetEnabled(false);

  InitLayoutManager();
}

void KeywordEditorView::InitLayoutManager() {
  const int related_x = kRelatedControlHorizontalSpacing;
  const int related_y = kRelatedControlVerticalSpacing;
  const int unrelated_y = kUnrelatedControlVerticalSpacing;

  GridLayout* contents_layout = GridLayout::CreatePanel(this);
  SetLayoutManager(contents_layout);

  // For the table and buttons.
  views::ColumnSet* column_set = contents_layout->AddColumnSet(0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, related_x);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  contents_layout->StartRow(0, 0);
  contents_layout->AddView(table_view_, 1, 8, GridLayout::FILL,
                           GridLayout::FILL);
  contents_layout->AddView(add_button_);

  contents_layout->StartRowWithPadding(0, 0, 0, related_y);
  contents_layout->SkipColumns(2);
  contents_layout->AddView(edit_button_);

  contents_layout->StartRowWithPadding(0, 0, 0, related_y);
  contents_layout->SkipColumns(2);
  contents_layout->AddView(remove_button_);

  contents_layout->StartRowWithPadding(0, 0, 0, related_y);
  contents_layout->SkipColumns(2);
  contents_layout->AddView(make_default_button_);

  contents_layout->AddPaddingRow(1, 0);
}

void KeywordEditorView::OnSelectionChanged() {
  bool only_one_url_left =
      controller_->url_model()->GetTemplateURLs().size() == 1;
  if (table_view_->SelectedRowCount() == 1) {
    const TemplateURL* selected_url =
        controller_->GetTemplateURL(table_view_->FirstSelectedRow());
    edit_button_->SetEnabled(controller_->CanEdit(selected_url));
    make_default_button_->SetEnabled(controller_->CanMakeDefault(selected_url));
    remove_button_->SetEnabled(!only_one_url_left &&
                               controller_->CanRemove(selected_url));
  } else {
    edit_button_->SetEnabled(false);
    make_default_button_->SetEnabled(false);
    for (views::TableView::iterator i = table_view_->SelectionBegin();
         i != table_view_->SelectionEnd(); ++i) {
      const TemplateURL* selected_url = controller_->GetTemplateURL(*i);
      if (!controller_->CanRemove(selected_url)) {
        remove_button_->SetEnabled(false);
        return;
      }
    }
    remove_button_->SetEnabled(!only_one_url_left);
  }
}

void KeywordEditorView::OnDoubleClick() {
  if (edit_button_->IsEnabled()) {
    DWORD pos = GetMessagePos();
    gfx::Point cursor_point(pos);
    views::MouseEvent event(views::Event::ET_MOUSE_RELEASED,
                            cursor_point.x(), cursor_point.y(),
                            views::Event::EF_LEFT_BUTTON_DOWN);
    ButtonPressed(edit_button_, event);
  }
}

void KeywordEditorView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == add_button_) {
    browser::EditSearchEngine(GetWindow()->GetNativeWindow(), NULL, this,
                              profile_);
  } else if (sender == remove_button_) {
    DCHECK_GT(table_view_->SelectedRowCount(), 0);
    int last_view_row = -1;
    for (views::TableView::iterator i = table_view_->SelectionBegin();
         i != table_view_->SelectionEnd(); ++i) {
      last_view_row = table_view_->ModelToView(*i);
      controller_->RemoveTemplateURL(*i);
    }
    if (last_view_row >= controller_->table_model()->RowCount())
      last_view_row = controller_->table_model()->RowCount() - 1;
    if (last_view_row >= 0)
      table_view_->Select(table_view_->ViewToModel(last_view_row));
  } else if (sender == edit_button_) {
    const int selected_row = table_view_->FirstSelectedRow();
    const TemplateURL* template_url =
        controller_->GetTemplateURL(selected_row);
    browser::EditSearchEngine(GetWindow()->GetNativeWindow(), template_url,
                              this, profile_);
  } else if (sender == make_default_button_) {
    MakeDefaultTemplateURL();
  } else {
    NOTREACHED();
  }
}

void KeywordEditorView::OnTemplateURLModelChanged() {
  add_button_->SetEnabled(controller_->loaded());
}

void KeywordEditorView::MakeDefaultTemplateURL() {
  int new_index =
      controller_->MakeDefaultTemplateURL(table_view_->FirstSelectedRow());
  if (new_index >= 0)
    table_view_->Select(new_index);
  default_chosen_ = true;
}
