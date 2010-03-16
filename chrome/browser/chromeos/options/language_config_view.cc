// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/options/language_config_view.h"

#include <utility>
#include <vector>

#include "app/l10n_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/language_library.h"
#include "chrome/browser/chromeos/options/language_hangul_config_view.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "views/controls/button/checkbox.h"
#include "views/controls/label.h"
#include "views/grid_layout.h"
#include "views/standard_layout.h"
#include "views/window/window.h"

namespace chromeos {

// This is a checkbox associated with input language information.
class LanguageCheckbox : public views::Checkbox {
 public:
  explicit LanguageCheckbox(const InputLanguage& language)
      : views::Checkbox(UTF8ToWide(language.display_name)),
        language_(language) {
  }

  const InputLanguage& language() const {
    return language_;
  }

 private:
  InputLanguage language_;
  DISALLOW_COPY_AND_ASSIGN(LanguageCheckbox);
};

LanguageConfigView::LanguageConfigView() :
    contents_(NULL),
    hangul_configure_button_(NULL),
    language_checkbox_(NULL),
    preferred_language_table_(NULL) {
}

LanguageConfigView::~LanguageConfigView() {
}

void LanguageConfigView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (sender == static_cast<views::Button*>(hangul_configure_button_)) {
    views::Window* window = views::Window::CreateChromeWindow(
        NULL, gfx::Rect(), new LanguageHangulConfigView());
    window->SetIsAlwaysOnTop(true);
    window->Show();
  } else if (sender == static_cast<views::Button*>(language_checkbox_)) {
    LanguageCheckbox* checkbox = static_cast<LanguageCheckbox*>(sender);
    const InputLanguage& language = checkbox->language();
    // Check if the checkbox is now being checked.
    if (checkbox->checked()) {
      // TODO(yusukes): limit the number of active languages so the pop-up menu
      //   of the language_menu_button does not overflow.

      // Try to activate the language.
      if (!LanguageLibrary::Get()->ActivateLanguage(language.category,
                                                    language.id)) {
        // Activation should never fail (failure means something is broken),
        // but if it fails we just revert the checkbox and ignore the error.
        // TODO(satorux): Implement a better error handling if it becomes
        // necessary.
        checkbox->SetChecked(false);
        LOG(ERROR) << "Failed to activate language: "
                   << language.display_name;
      }
    } else {
      // Try to deactivate the language.
      if (!LanguageLibrary::Get()->DeactivateLanguage(language.category,
                                                      language.id)) {
        // See a comment above about the activation failure.
        checkbox->SetChecked(true);
        LOG(ERROR) << "Failed to deactivate language: "
                   << language.display_name;
      }
    }
  }
}

void LanguageConfigView::Layout() {
  // Not sure why but this is needed to show contents in the dialog.
  contents_->SetBounds(0, 0, width(), height());
}

std::wstring LanguageConfigView::GetWindowTitle() const {
  return l10n_util::GetString(
      IDS_OPTIONS_SETTINGS_LANGUAGES_DIALOG_TITLE);
}

gfx::Size LanguageConfigView::GetPreferredSize() {
  // TODO(satorux): Create our own localized content size once the UI is done.
  return gfx::Size(views::Window::GetLocalizedContentsSize(
      IDS_FONTSLANG_DIALOG_WIDTH_CHARS,
      IDS_FONTSLANG_DIALOG_HEIGHT_LINES));
}

void LanguageConfigView::ViewHierarchyChanged(
    bool is_add, views::View* parent, views::View* child) {
  // Can't init before we're inserted into a Container.
  if (is_add && child == this) {
    Init();
  }
}

void LanguageConfigView::OnSelectionChanged() {
  // TODO(satorux): Implement this.
}

std::wstring LanguageConfigView::GetText(int row, int column_id) {
  return L"Not yet implemented";
}

void LanguageConfigView::SetObserver(TableModelObserver* observer) {
}

int LanguageConfigView::RowCount() {
  return 1;
}


// TODO(satorux): Refactor the function.
void LanguageConfigView::Init() {
  using views::ColumnSet;
  using views::GridLayout;

  if (contents_) return;  // Already initialized.
  contents_ = new views::View;
  AddChildView(contents_);

  // Set up the layout manager for the outer contents.  We'll place the
  // preferred language table on the left, and anything else on the right.
  GridLayout* outer_layout = new GridLayout(contents_);
  contents_->SetLayoutManager(outer_layout);
  outer_layout->SetInsets(kPanelVertMargin, kPanelHorizMargin,
                          kPanelVertMargin, kPanelHorizMargin);

  // Set up the column set for the outer contents.
  const int kOuterColumnSetId = 0;
  ColumnSet* column_set = outer_layout->AddColumnSet(kOuterColumnSetId);
  column_set->AddColumn(GridLayout::FILL, GridLayout::FILL, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::LEADING, 0,
                        GridLayout::USE_PREF, 0, 0);

  // Set up the preferred language table.
  std::vector<TableColumn> columns;
  TableColumn column(0,
                     l10n_util::GetString(
                         IDS_OPTIONS_SETTINGS_LANGUAGES_LANGUAGES),
                     TableColumn::LEFT, -1 /* width */, 0 /* percent */);
  columns.push_back(column);
  preferred_language_table_ =
      new views::TableView2(this,
                            columns,
                            views::TEXT_ONLY,
                            true, true, true);
  preferred_language_table_->SetObserver(this);

  // Add the preferred language table.
  outer_layout->StartRow(1 /* expand */, kOuterColumnSetId);
  outer_layout->AddView(preferred_language_table_);

  // Set up the container for the contents on the right.
  views::View* right_contents = new views::View;
  GridLayout* layout = new GridLayout(right_contents);
  right_contents->SetLayoutManager(layout);
  outer_layout->AddView(right_contents);

  // Set up the single column set.
  const int kSingleColumnSetId = 1;
  column_set = layout->AddColumnSet(kSingleColumnSetId);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  // Set up the double column set.
  const int kDoubleColumnSetId = 2;
  column_set = layout->AddColumnSet(kDoubleColumnSetId);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, kRelatedControlHorizontalSpacing);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);

  // GetActiveLanguages() and GetSupportedLanguages() never return NULL.
  scoped_ptr<InputLanguageList> active_language_list(
      LanguageLibrary::Get()->GetActiveLanguages());
  scoped_ptr<InputLanguageList> supported_language_list(
      LanguageLibrary::Get()->GetSupportedLanguages());

  for (size_t i = 0; i < supported_language_list->size(); ++i) {
    const InputLanguage& language = supported_language_list->at(i);
    // Check if |language| is active. Note that active_language_list is
    // small (usually a couple), so scanning here is fine.
    const bool language_is_active =
        (std::find(active_language_list->begin(),
                   active_language_list->end(),
                   language) != active_language_list->end());
    // Create a checkbox.
    language_checkbox_ = new LanguageCheckbox(language);
    language_checkbox_->SetChecked(language_is_active);
    language_checkbox_->set_listener(this);

    // We use two columns. Start a column if the counter is an even number.
    if (i % 2 == 0) {
      layout->StartRow(0, kDoubleColumnSetId);
    }
    // Add the checkbox to the layout manager.
    layout->AddView(language_checkbox_);
  }

  // Add the configure button for the Hangul IME.
  // TODO(satorux): We'll have better UI for this soon.
  layout->StartRow(0, kDoubleColumnSetId);
  hangul_configure_button_ = new views::NativeButton(
      this, l10n_util::GetString(IDS_OPTIONS_SETTINGS_LANGUAGES_CONFIGURE));
  layout->AddView(new views::Label(
      l10n_util::GetString(IDS_OPTIONS_SETTINGS_HANGUL_IME_TEXT)));
  layout->AddView(hangul_configure_button_);
}

}  // namespace chromeos
