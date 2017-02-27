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
#include "components/payments/content/payment_request.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/native_theme/native_theme.h"
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
  // No insets. Child views below are responsible for their padding.

  // An editor can optionally have a header view specific to it.
  content_view->AddChildView(CreateHeaderView().release());

  // The heart of the editor dialog: all the input fields with their labels.
  content_view->AddChildView(CreateEditorView().release());

  return CreatePaymentView(
      CreateSheetHeaderView(
          true, l10n_util::GetStringUTF16(
                    IDS_PAYMENT_REQUEST_CREDIT_CARD_EDITOR_ADD_TITLE),
          this),
      std::move(content_view));
}

// Adds the "required fields" label in disabled text, to obtain this result.
// +---------------------------------------------------------+
// | "* indicates required fields"           | CANCEL | DONE |
// +---------------------------------------------------------+
std::unique_ptr<views::View> EditorViewController::CreateExtraFooterView() {
  std::unique_ptr<views::View> content_view = base::MakeUnique<views::View>();

  views::BoxLayout* layout =
      new views::BoxLayout(views::BoxLayout::kHorizontal, 0, 0, 0);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  content_view->SetLayoutManager(layout);

  // Adds the "* indicates a required field" label in "disabled" grey text.
  std::unique_ptr<views::Label> label = base::MakeUnique<views::Label>(
      l10n_util::GetStringUTF16(IDS_PAYMENTS_REQUIRED_FIELD_MESSAGE));
  label->SetDisabledColor(label->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_LabelDisabledColor));
  label->SetEnabled(false);
  content_view->AddChildView(label.release());
  return content_view;
}

void EditorViewController::DisplayErrorMessageForField(
    const EditorField& field,
    const base::string16& error_message) {
  const auto& label_it = error_labels_.find(field);
  DCHECK(label_it != error_labels_.end());
  label_it->second->SetText(error_message);
  label_it->second->SchedulePaint();
  dialog()->Layout();
}

std::unique_ptr<views::Button> EditorViewController::CreatePrimaryButton() {
  std::unique_ptr<views::Button> button(
      views::MdTextButton::CreateSecondaryUiBlueButton(
          this, l10n_util::GetStringUTF16(IDS_DONE)));
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

std::unique_ptr<views::View> EditorViewController::CreateEditorView() {
  std::unique_ptr<views::View> editor_view = base::MakeUnique<views::View>();

  views::GridLayout* editor_layout = new views::GridLayout(editor_view.get());

  // The editor grid layout is padded vertically from the top and bottom, and
  // horizontally inset like other content views. The top padding needs to be
  // added to the top padding of the first row.
  constexpr int kEditorVerticalInset = 16;
  editor_layout->SetInsets(
      kEditorVerticalInset, payments::kPaymentRequestRowHorizontalInsets,
      kEditorVerticalInset, payments::kPaymentRequestRowHorizontalInsets);

  editor_view->SetLayoutManager(editor_layout);
  views::ColumnSet* columns = editor_layout->AddColumnSet(0);
  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 0,
                     views::GridLayout::USE_PREF, 0, 0);

  // This is the horizontal padding between the label and the input field.
  constexpr int kLabelInputFieldHorizontalPadding = 16;
  columns->AddPaddingColumn(0, kLabelInputFieldHorizontalPadding);

  columns->AddColumn(views::GridLayout::LEADING, views::GridLayout::CENTER, 0,
                     views::GridLayout::USE_PREF, 0, 0);

  std::vector<EditorField> fields = GetFieldDefinitions();
  for (const auto& field : fields) {
    CreateInputField(editor_layout, field);
  }

  return editor_view;
}

// Each input field is a 4-quadrant grid.
// +----------------------------------------------------------+
// | Field Label           | Input field (textfield/combobox) |
// |_______________________|__________________________________|
// |   (empty)             | Error label                      |
// +----------------------------------------------------------+
void EditorViewController::CreateInputField(views::GridLayout* layout,
                                            const EditorField& field) {
  // This is the top padding for every row.
  constexpr int kInputRowSpacing = 6;
  layout->StartRowWithPadding(0, 0, 0, kInputRowSpacing);

  std::unique_ptr<views::Label> label = base::MakeUnique<views::Label>(
      field.required ? field.label + base::ASCIIToUTF16("*") : field.label);
  // A very long label will wrap. Value picked so that left + right label
  // padding bring the label to half-way in the dialog (~225).
  constexpr int kMaximumLabelWidth = 192;
  label->SetMultiLine(true);
  label->SetMaximumWidth(kMaximumLabelWidth);
  layout->AddView(label.release());

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

  // This is the vertical space between the input field and its error label.
  constexpr int kInputErrorLabelPadding = 6;
  layout->StartRowWithPadding(0, 0, 0, kInputErrorLabelPadding);
  layout->SkipColumns(1);
  // Error label is initially empty.
  std::unique_ptr<views::Label> error_label =
      base::MakeUnique<views::Label>(base::ASCIIToUTF16(""));
  error_label->set_id(static_cast<int>(DialogViewID::ERROR_LABEL_OFFSET) +
                      field.type);
  error_label->SetFontList(
      error_label->GetDefaultFontList().DeriveWithSizeDelta(-1));
  error_label->SetEnabledColor(error_label->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_AlertSeverityHigh));
  error_labels_[field] = error_label.get();

  layout->AddView(error_label.release());
}

}  // namespace payments
