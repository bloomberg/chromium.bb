// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/edit_search_engine_dialog.h"

#include "base/i18n/case_conversion.h"
#include "base/i18n/rtl.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/search_engines/ui_thread_search_terms_data.h"
#include "chrome/browser/ui/search_engines/edit_search_engine_controller.h"
#include "chrome/browser/ui/views/constrained_window_views.h"
#include "chrome/grit/generated_resources.h"
#include "components/search_engines/template_url.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/events/event.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/layout/layout_constants.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/dialog_client_view.h"
#include "url/gurl.h"

using views::GridLayout;
using views::Textfield;

namespace chrome {

void EditSearchEngine(gfx::NativeWindow parent,
                      TemplateURL* template_url,
                      EditSearchEngineControllerDelegate* delegate,
                      Profile* profile) {
  EditSearchEngineDialog::Show(parent, template_url, delegate, profile);
}

}  // namespace chrome

EditSearchEngineDialog::EditSearchEngineDialog(
    TemplateURL* template_url,
    EditSearchEngineControllerDelegate* delegate,
    Profile* profile)
    : controller_(new EditSearchEngineController(template_url,
                                                 delegate,
                                                 profile)) {
  Init();
}

EditSearchEngineDialog::~EditSearchEngineDialog() {
}

// static
void EditSearchEngineDialog::Show(gfx::NativeWindow parent,
                                  TemplateURL* template_url,
                                  EditSearchEngineControllerDelegate* delegate,
                                  Profile* profile) {
  EditSearchEngineDialog* contents =
      new EditSearchEngineDialog(template_url, delegate, profile);
  // Window interprets an empty rectangle as needing to query the content for
  // the size as well as centering relative to the parent.
  CreateBrowserModalDialogViews(contents, parent);
  contents->GetWidget()->Show();
  contents->GetDialogClientView()->UpdateDialogButtons();
  contents->title_tf_->SelectAll(true);
  contents->title_tf_->RequestFocus();
}

ui::ModalType EditSearchEngineDialog::GetModalType() const {
  return ui::MODAL_TYPE_WINDOW;
}

base::string16 EditSearchEngineDialog::GetWindowTitle() const {
  return l10n_util::GetStringUTF16(controller_->template_url() ?
      IDS_SEARCH_ENGINES_EDITOR_EDIT_WINDOW_TITLE :
      IDS_SEARCH_ENGINES_EDITOR_NEW_WINDOW_TITLE);
}

bool EditSearchEngineDialog::IsDialogButtonEnabled(
    ui::DialogButton button) const {
  if (button == ui::DIALOG_BUTTON_OK) {
    return (controller_->IsKeywordValid(keyword_tf_->text()) &&
            controller_->IsTitleValid(title_tf_->text()) &&
            controller_->IsURLValid(base::UTF16ToUTF8(url_tf_->text())));
  }
  return true;
}

bool EditSearchEngineDialog::Cancel() {
  controller_->CleanUpCancelledAdd();
  return true;
}

bool EditSearchEngineDialog::Accept() {
  controller_->AcceptAddOrEdit(title_tf_->text(), keyword_tf_->text(),
                               base::UTF16ToUTF8(url_tf_->text()));
  return true;
}

void EditSearchEngineDialog::ContentsChanged(
    Textfield* sender,
    const base::string16& new_contents) {
  // Force the keyword text to be lowercase, keep the caret or selection.
  if (sender == keyword_tf_ && !sender->HasCompositionText()) {
    // TODO(msw): Prevent textfield scrolling with selection model changes here.
    const gfx::SelectionModel selection_model = sender->GetSelectionModel();
    sender->SetText(base::i18n::ToLower(new_contents));
    sender->SelectSelectionModel(selection_model);
  }

  GetDialogClientView()->UpdateDialogButtons();
  UpdateImageViews();
}

bool EditSearchEngineDialog::HandleKeyEvent(
    Textfield* sender,
    const ui::KeyEvent& key_event) {
  return false;
}

void EditSearchEngineDialog::Init() {
  // Create the views we'll need.
  if (controller_->template_url()) {
    title_tf_ = CreateTextfield(controller_->template_url()->short_name());
    keyword_tf_ = CreateTextfield(controller_->template_url()->keyword());
    url_tf_ = CreateTextfield(
        controller_->template_url()->url_ref().DisplayURL(
            UIThreadSearchTermsData(controller_->profile())));
    // We don't allow users to edit prepopulate URLs. This is done as
    // occasionally we need to update the URL of prepopulated TemplateURLs.
    url_tf_->SetReadOnly(controller_->template_url()->prepopulate_id() != 0);
  } else {
    title_tf_ = CreateTextfield(base::string16());
    keyword_tf_ = CreateTextfield(base::string16());
    url_tf_ = CreateTextfield(base::string16());
  }
  title_iv_ = new views::ImageView();
  keyword_iv_ = new views::ImageView();
  url_iv_ = new views::ImageView();

  UpdateImageViews();

  const int related_x = views::kRelatedControlHorizontalSpacing;
  const int related_y = views::kRelatedControlVerticalSpacing;
  const int unrelated_y = views::kUnrelatedControlVerticalSpacing;

  // View and GridLayout take care of deleting GridLayout for us.
  GridLayout* layout = GridLayout::CreatePanel(this);
  SetLayoutManager(layout);

  // Define the structure of the layout.

  // For the buttons.
  views::ColumnSet* column_set = layout->AddColumnSet(0);
  column_set->AddPaddingColumn(1, 0);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, related_x);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->LinkColumnSizes(1, 3, -1);

  // For the Textfields.
  column_set = layout->AddColumnSet(1);
  column_set->AddColumn(GridLayout::LEADING, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, related_x);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 1,
                        GridLayout::USE_PREF, 0, 0);
  column_set->AddPaddingColumn(0, related_x);
  column_set->AddColumn(GridLayout::CENTER, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  // For the description.
  column_set = layout->AddColumnSet(2);
  column_set->AddColumn(GridLayout::FILL, GridLayout::CENTER, 0,
                        GridLayout::USE_PREF, 0, 0);

  // Add the contents.
  layout->StartRow(0, 1);
  layout->AddView(CreateLabel(IDS_SEARCH_ENGINES_EDITOR_DESCRIPTION_LABEL));
  layout->AddView(title_tf_);
  layout->AddView(title_iv_);

  layout->StartRowWithPadding(0, 1, 0, related_y);
  layout->AddView(CreateLabel(IDS_SEARCH_ENGINES_EDITOR_KEYWORD_LABEL));
  layout->AddView(keyword_tf_);
  layout->AddView(keyword_iv_);

  layout->StartRowWithPadding(0, 1, 0, related_y);
  layout->AddView(CreateLabel(IDS_SEARCH_ENGINES_EDITOR_URL_LABEL));
  layout->AddView(url_tf_);
  layout->AddView(url_iv_);

  // On RTL UIs (such as Arabic and Hebrew) the description text is not
  // displayed correctly since it contains the substring "%s". This substring
  // is not interpreted by the Unicode BiDi algorithm as an LTR string and
  // therefore the end result is that the following right to left text is
  // displayed: ".three two s% one" (where 'one', 'two', etc. are words in
  // Hebrew).
  //
  // In order to fix this problem we transform the substring "%s" so that it
  // is displayed correctly when rendered in an RTL context.
  layout->StartRowWithPadding(0, 2, 0, unrelated_y);
  base::string16 description = l10n_util::GetStringUTF16(
      IDS_SEARCH_ENGINES_EDITOR_URL_DESCRIPTION_LABEL);
  if (base::i18n::IsRTL()) {
    const base::string16 reversed_percent(base::ASCIIToUTF16("s%"));
    base::string16::size_type percent_index =
        description.find(base::ASCIIToUTF16("%s"),
                         static_cast<base::string16::size_type>(0));
    if (percent_index != base::string16::npos)
      description.replace(percent_index,
                          reversed_percent.length(),
                          reversed_percent);
  }

  views::Label* description_label = new views::Label(description);
  description_label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  layout->AddView(description_label);

  layout->AddPaddingRow(0, related_y);
}

views::Label* EditSearchEngineDialog::CreateLabel(int message_id) {
  views::Label* label =
      new views::Label(l10n_util::GetStringUTF16(message_id));
  label->SetHorizontalAlignment(gfx::ALIGN_LEFT);
  return label;
}

Textfield* EditSearchEngineDialog::CreateTextfield(const base::string16& text) {
  Textfield* text_field = new Textfield();
  text_field->SetText(text);
  text_field->set_controller(this);
  return text_field;
}

void EditSearchEngineDialog::UpdateImageViews() {
  UpdateImageView(keyword_iv_, controller_->IsKeywordValid(keyword_tf_->text()),
                  IDS_SEARCH_ENGINES_INVALID_KEYWORD_TT);
  UpdateImageView(url_iv_,
                  controller_->IsURLValid(base::UTF16ToUTF8(url_tf_->text())),
                  IDS_SEARCH_ENGINES_INVALID_URL_TT);
  UpdateImageView(title_iv_, controller_->IsTitleValid(title_tf_->text()),
                  IDS_SEARCH_ENGINES_INVALID_TITLE_TT);
}

void EditSearchEngineDialog::UpdateImageView(views::ImageView* image_view,
                                             bool is_valid,
                                             int invalid_message_id) {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (is_valid) {
    image_view->SetTooltipText(base::string16());
    image_view->SetImage(rb.GetImageSkiaNamed(IDR_INPUT_GOOD));
  } else {
    image_view->SetTooltipText(l10n_util::GetStringUTF16(invalid_message_id));
    image_view->SetImage(rb.GetImageSkiaNamed(IDR_INPUT_ALERT));
  }
}
