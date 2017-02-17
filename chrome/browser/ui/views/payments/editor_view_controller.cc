// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/editor_view_controller.h"

#include <memory>
#include <utility>

#include "base/memory/ptr_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/browser/ui/views/payments/validating_combobox.h"
#include "chrome/browser/ui/views/payments/validating_textfield.h"
#include "chrome/grit/generated_resources.h"
#include "components/payments/payment_request.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/views/border.h"
#include "ui/views/controls/button/label_button.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/styled_label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace payments {
namespace {

constexpr int kFirstTagValue = static_cast<int>(
    payments::PaymentRequestCommonTags::PAYMENT_REQUEST_COMMON_TAG_MAX);

enum class EditorViewControllerTags : int {
  // The tag for the button that saves the model being edited. Starts at
  // |kFirstTagValue| not to conflict with tags common to all views.
  SAVE_BUTTON = kFirstTagValue,
};

constexpr int kNumCharactersInShortField = 8;
constexpr int kNumCharactersInLongField = 20;

}  // namespace

EditorViewController::EditorViewController(PaymentRequest* request,
                                           PaymentRequestDialogView* dialog)
    : PaymentRequestSheetController(request, dialog) {}

EditorViewController::~EditorViewController() {}

std::unique_ptr<views::View> EditorViewController::CreateView() {
  std::unique_ptr<views::View> content_view = base::MakeUnique<views::View>();

  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_STRETCH);
  content_view->SetLayoutManager(layout);

  content_view->AddChildView(CreateHeaderView().release());

  // Create an input label/textfield for each field definition.
  std::vector<EditorField> fields = GetFieldDefinitions();
  for (const auto& field : fields) {
    content_view->AddChildView(CreateInputField(field).release());
  }

  return CreatePaymentView(
      CreateSheetHeaderView(
          true, l10n_util::GetStringUTF16(
                    IDS_PAYMENT_REQUEST_CREDIT_CARD_EDITOR_ADD_TITLE),
          this),
      std::move(content_view));
}

std::unique_ptr<views::Button> EditorViewController::CreatePrimaryButton() {
  std::unique_ptr<views::Button> button(
      views::MdTextButton::CreateSecondaryUiBlueButton(
          this, l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_SAVE_BUTTON)));
  button->set_tag(static_cast<int>(EditorViewControllerTags::SAVE_BUTTON));
  button->set_id(static_cast<int>(DialogViewID::EDITOR_SAVE_BUTTON));
  return button;
}

void EditorViewController::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  switch (sender->tag()) {
    case static_cast<int>(EditorViewControllerTags::SAVE_BUTTON):
      if (ValidateModelAndSave())
        dialog()->GoBack();
      break;
    default:
      PaymentRequestSheetController::ButtonPressed(sender, event);
      break;
  }
}

void EditorViewController::ContentsChanged(views::Textfield* sender,
                                           const base::string16& new_contents) {
  static_cast<ValidatingTextfield*>(sender)->OnContentsChanged();
}

void EditorViewController::OnPerformAction(views::Combobox* sender) {
  static_cast<ValidatingCombobox*>(sender)->OnContentsChanged();
}

std::unique_ptr<views::View> EditorViewController::CreateInputField(
    const EditorField& field) {
  std::unique_ptr<views::View> row = base::MakeUnique<views::View>();

  row->SetBorder(payments::CreatePaymentRequestRowBorder());

  views::GridLayout* layout = new views::GridLayout(row.get());

  // The vertical spacing for these rows is slightly different than the spacing
  // spacing for clickable rows, so don't use kPaymentRequestRowVerticalInsets.
  constexpr int kRowVerticalInset = 12;
  layout->SetInsets(
      kRowVerticalInset, payments::kPaymentRequestRowHorizontalInsets,
      kRowVerticalInset, payments::kPaymentRequestRowHorizontalInsets);

  row->SetLayoutManager(layout);
  views::ColumnSet* columns = layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 0,
                     views::GridLayout::USE_PREF, 0, 0);
  columns->AddPaddingColumn(1, 0);
  columns->AddColumn(views::GridLayout::TRAILING, views::GridLayout::CENTER, 0,
                     views::GridLayout::USE_PREF, 0, 0);

  layout->StartRow(0, 0);
  layout->AddView(new views::Label(field.label));

  if (field.control_type == EditorField::ControlType::TEXTFIELD) {
    ValidatingTextfield* text_field =
        new ValidatingTextfield(CreateValidationDelegate(field));
    text_field->set_controller(this);
    // Using autofill field type as a view ID (for testing).
    text_field->set_id(static_cast<int>(field.type));
    text_field->set_default_width_in_chars(
        field.length_hint == EditorField::LengthHint::HINT_SHORT
            ? kNumCharactersInShortField
            : kNumCharactersInLongField);

    text_fields_.insert(std::make_pair(text_field, field));
    // |text_field| will now be owned by |row|.
    layout->AddView(text_field);
  } else if (field.control_type == EditorField::ControlType::COMBOBOX) {
    ValidatingCombobox* combobox = new ValidatingCombobox(
        GetComboboxModelForType(field.type), CreateValidationDelegate(field));
    // Using autofill field type as a view ID (for testing).
    combobox->set_id(static_cast<int>(field.type));
    combobox->set_listener(this);
    comboboxes_.insert(std::make_pair(combobox, field));
    // |combobox| will now be owned by |row|.
    layout->AddView(combobox);
  } else {
    NOTREACHED();
  }

  return row;
}

}  // namespace payments
