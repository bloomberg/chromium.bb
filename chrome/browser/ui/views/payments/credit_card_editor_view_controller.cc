// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/payments/credit_card_editor_view_controller.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "base/bind.h"
#include "base/memory/ptr_util.h"
#include "base/strings/string16.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view.h"
#include "chrome/browser/ui/views/payments/payment_request_dialog_view_ids.h"
#include "chrome/browser/ui/views/payments/payment_request_views_util.h"
#include "chrome/browser/ui/views/payments/preselected_combobox_model.h"
#include "chrome/browser/ui/views/payments/validating_combobox.h"
#include "chrome/browser/ui/views/payments/validating_textfield.h"
#include "chrome/grit/generated_resources.h"
#include "components/autofill/core/browser/address_combobox_model.h"
#include "components/autofill/core/browser/autofill_data_util.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/credit_card.h"
#include "components/autofill/core/browser/field_types.h"
#include "components/autofill/core/browser/personal_data_manager.h"
#include "components/autofill/core/browser/validation.h"
#include "components/autofill/core/common/autofill_clock.h"
#include "components/autofill/core/common/autofill_constants.h"
#include "components/payments/content/payment_request_spec.h"
#include "components/payments/content/payment_request_state.h"
#include "components/strings/grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/native_theme/native_theme.h"
#include "ui/views/controls/button/md_text_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/grid_layout.h"
#include "ui/views/view.h"

namespace payments {

namespace {

// Number of years (including the current one) to be shown in the expiration
// year dropdown.
const int kNumberOfExpirationYears = 10;

// This is not quite right but is the closest server type that wasn't already
// used.
const auto kBillingAddressType = autofill::ADDRESS_BILLING_LINE1;

// Returns the items that are in the expiration month dropdown. Will return the
// months in order starting at "01" until "12". Uses a clock so that the
// |default_index| is set to the current month.
std::vector<base::string16> GetExpirationMonthItems(int* default_index) {
  std::vector<base::string16> months;
  months.reserve(12);
  for (int i = 1; i <= 12; i++)
    months.push_back(base::UTF8ToUTF16(base::StringPrintf("%02d", i)));

  base::Time::Exploded now_exploded;
  autofill::AutofillClock::Now().LocalExplode(&now_exploded);
  *default_index = now_exploded.month - 1;

  return months;
}

// Returns the items that are in the expiration year dropdown, with the first
// year being the current year.
std::vector<base::string16> GetExpirationYearItems() {
  std::vector<base::string16> years;
  years.reserve(kNumberOfExpirationYears);

  base::Time::Exploded now_exploded;
  autofill::AutofillClock::Now().LocalExplode(&now_exploded);
  for (int i = 0; i < kNumberOfExpirationYears; i++) {
    years.push_back(base::UTF8ToUTF16(std::to_string(now_exploded.year + i)));
  }
  return years;
}

}  // namespace

CreditCardEditorViewController::CreditCardEditorViewController(
    PaymentRequestSpec* spec,
    PaymentRequestState* state,
    PaymentRequestDialogView* dialog,
    BackNavigationType back_navigation,
    int next_ui_tag,
    base::OnceClosure on_edited,
    base::OnceCallback<void(const autofill::CreditCard&)> on_added,
    autofill::CreditCard* credit_card)
    : EditorViewController(spec, state, dialog, back_navigation),
      on_edited_(std::move(on_edited)),
      on_added_(std::move(on_added)),
      credit_card_to_edit_(credit_card),
      add_billing_address_button_tag_(next_ui_tag) {}

CreditCardEditorViewController::~CreditCardEditorViewController() {}

// Creates the "Cards accepted" view with a row of icons at the top of the
// credit card editor.
// +----------------------------------------------+
// | Cards Accepted                               |
// |                                              |
// | | VISA | | MC | | AMEX |                     |
// +----------------------------------------------+
std::unique_ptr<views::View>
CreditCardEditorViewController::CreateHeaderView() {
  std::unique_ptr<views::View> view = base::MakeUnique<views::View>();

  // 9dp is required between the first row (label) and second row (icons).
  constexpr int kRowVerticalSpacing = 9;
  // 6dp is added to the bottom padding, for a total of 12 between the icons and
  // the first input field.
  constexpr int kRowBottomPadding = 6;
  views::BoxLayout* layout = new views::BoxLayout(
      views::BoxLayout::kVertical, payments::kPaymentRequestRowHorizontalInsets,
      kRowBottomPadding, kRowVerticalSpacing);
  layout->set_main_axis_alignment(views::BoxLayout::MAIN_AXIS_ALIGNMENT_START);
  layout->set_cross_axis_alignment(
      views::BoxLayout::CROSS_AXIS_ALIGNMENT_START);
  view->SetLayoutManager(layout);

  // "Cards accepted" label is "disabled" grey.
  std::unique_ptr<views::Label> label = base::MakeUnique<views::Label>(
      l10n_util::GetStringUTF16(IDS_PAYMENTS_ACCEPTED_CARDS_LABEL));
  label->SetDisabledColor(label->GetNativeTheme()->GetSystemColor(
      ui::NativeTheme::kColorId_LabelDisabledColor));
  label->SetEnabled(false);
  view->AddChildView(label.release());

  // 8dp padding is required between icons.
  constexpr int kPaddingBetweenCardIcons = 8;
  std::unique_ptr<views::View> icons_row = base::MakeUnique<views::View>();
  views::BoxLayout* icons_layout = new views::BoxLayout(
      views::BoxLayout::kHorizontal, 0, 0, kPaddingBetweenCardIcons);
  icons_row->SetLayoutManager(icons_layout);

  constexpr gfx::Size kCardIconSize = gfx::Size(30, 18);
  for (const std::string& supported_network :
       spec()->supported_card_networks()) {
    const std::string autofill_card_type =
        autofill::data_util::GetIssuerNetworkForBasicCardIssuerNetwork(
            supported_network);
    std::unique_ptr<views::ImageView> card_icon_view = CreateInstrumentIconView(
        autofill::data_util::GetPaymentRequestData(autofill_card_type)
            .icon_resource_id,
        base::UTF8ToUTF16(supported_network));
    card_icon_view->SetImageSize(kCardIconSize);

    icons_row->AddChildView(card_icon_view.release());
  }
  view->AddChildView(icons_row.release());

  return view;
}

std::unique_ptr<views::View>
CreditCardEditorViewController::CreateExtraViewForField(
    autofill::ServerFieldType type) {
  if (type != kBillingAddressType)
    return nullptr;

  std::unique_ptr<views::View> button_view = base::MakeUnique<views::View>();
  button_view->SetLayoutManager(new views::FillLayout);

  // The button to add new billing addresses.
  std::unique_ptr<views::Button> add_button(
      views::MdTextButton::Create(this, l10n_util::GetStringUTF16(IDS_ADD)));
  add_button->set_id(
      static_cast<int>(DialogViewID::ADD_BILLING_ADDRESS_BUTTON));
  add_button->set_tag(add_billing_address_button_tag_);
  button_view->AddChildView(add_button.release());
  return button_view;
}

std::vector<EditorField> CreditCardEditorViewController::GetFieldDefinitions() {
  return std::vector<EditorField>{
      {autofill::CREDIT_CARD_NUMBER,
       l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_CREDIT_CARD_NUMBER),
       EditorField::LengthHint::HINT_SHORT, /* required= */ true},
      {autofill::CREDIT_CARD_NAME_FULL,
       l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_NAME_ON_CARD),
       EditorField::LengthHint::HINT_SHORT, /* required= */ true},
      {autofill::CREDIT_CARD_EXP_MONTH,
       l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_EXPIRATION_MONTH),
       EditorField::LengthHint::HINT_SHORT, /* required= */ true,
       EditorField::ControlType::COMBOBOX},
      {autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR,
       l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_EXPIRATION_YEAR),
       EditorField::LengthHint::HINT_SHORT, /* required= */ true,
       EditorField::ControlType::COMBOBOX},
      {kBillingAddressType,
       l10n_util::GetStringUTF16(IDS_AUTOFILL_FIELD_LABEL_BILLING_ADDRESS),
       EditorField::LengthHint::HINT_SHORT, /* required= */ true,
       EditorField::ControlType::COMBOBOX}};
}

base::string16 CreditCardEditorViewController::GetInitialValueForType(
    autofill::ServerFieldType type) {
  if (!credit_card_to_edit_ || type == kBillingAddressType)
    return base::string16();

  return credit_card_to_edit_->GetInfo(autofill::AutofillType(type),
                                       state()->GetApplicationLocale());
}

bool CreditCardEditorViewController::ValidateModelAndSave() {
  const std::string& locale = state()->GetApplicationLocale();
  // Use a temporary object for validation.
  autofill::CreditCard credit_card;
  credit_card.set_origin(autofill::kSettingsOrigin);

  for (const auto& field : text_fields()) {
    // ValidatingTextfield* is the key, EditorField is the value.
    DCHECK_EQ(autofill::CREDIT_CARD,
              autofill::AutofillType(field.second.type).group());
    if (field.first->invalid())
      return false;

    credit_card.SetInfo(autofill::AutofillType(field.second.type),
                        field.first->text(), locale);
  }
  for (const auto& field : comboboxes()) {
    // ValidatingCombobox* is the key, EditorField is the value.
    ValidatingCombobox* combobox = field.first;
    if (combobox->invalid())
      return false;

    if (field.second.type == kBillingAddressType) {
      views::Combobox* address_combobox = static_cast<views::Combobox*>(
          dialog()->GetViewByID(kBillingAddressType));
      autofill::AddressComboboxModel* model =
          static_cast<autofill::AddressComboboxModel*>(
              address_combobox->model());

      credit_card.set_billing_address_id(
          model->GetItemIdentifierAt(address_combobox->selected_index()));
    } else {
      credit_card.SetInfo(autofill::AutofillType(field.second.type),
                          combobox->GetTextForRow(combobox->selected_index()),
                          locale);
    }
  }

  // TODO(crbug.com/711365): Display global error message.
  if (autofill::GetCompletionStatusForCard(
          credit_card, locale,
          state()->GetPersonalDataManager()->GetProfiles()) !=
      autofill::CREDIT_CARD_COMPLETE) {
    return false;
  }

  if (!credit_card_to_edit_) {
    // Add the card (will not add a duplicate).
    state()->GetPersonalDataManager()->AddCreditCard(credit_card);
    std::move(on_added_).Run(credit_card);
  } else {
    credit_card_to_edit_->set_billing_address_id(
        credit_card.billing_address_id());
    // We were in edit mode. Copy the data from the temporary object to retain
    // the edited object's other properties (use count, use date, guid, etc.).
    for (const auto& field : text_fields()) {
      credit_card_to_edit_->SetInfo(
          autofill::AutofillType(field.second.type),
          credit_card.GetInfo(autofill::AutofillType(field.second.type),
                              locale),
          locale);
    }
    for (const auto& field : comboboxes()) {
      // The billing address is transfered above.
      if (field.second.type == kBillingAddressType)
        continue;

      credit_card_to_edit_->SetInfo(
          autofill::AutofillType(field.second.type),
          credit_card.GetInfo(autofill::AutofillType(field.second.type),
                              locale),
          locale);
    }
    state()->GetPersonalDataManager()->UpdateCreditCard(*credit_card_to_edit_);
    std::move(on_edited_).Run();
  }

  return true;
}

std::unique_ptr<ValidationDelegate>
CreditCardEditorViewController::CreateValidationDelegate(
    const EditorField& field) {
  // The supported card networks for non-cc-number types are not passed to avoid
  // the data copy in the delegate.
  return base::MakeUnique<
      CreditCardEditorViewController::CreditCardValidationDelegate>(
      field, this,
      field.type == autofill::CREDIT_CARD_NUMBER
          ? spec()->supported_card_networks()
          : std::vector<std::string>());
}

std::unique_ptr<ui::ComboboxModel>
CreditCardEditorViewController::GetComboboxModelForType(
    const autofill::ServerFieldType& type) {
  switch (type) {
    case autofill::CREDIT_CARD_EXP_MONTH: {
      int default_index = 0;
      std::vector<base::string16> months =
          GetExpirationMonthItems(&default_index);
      return base::MakeUnique<PreselectedComboboxModel>(months, default_index);
    }
    case autofill::CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return base::MakeUnique<ui::SimpleComboboxModel>(
          GetExpirationYearItems());
    case kBillingAddressType:
      // The combobox filled with potential billing addresses. It's fine to pass
      // empty string as the default selected guid if there are no cards being
      // edited.
      return base::MakeUnique<autofill::AddressComboboxModel>(
          *state()->GetPersonalDataManager(), state()->GetApplicationLocale(),
          credit_card_to_edit_ ? credit_card_to_edit_->billing_address_id()
                               : "");
    default:
      NOTREACHED();
      break;
  }
  return std::unique_ptr<ui::ComboboxModel>();
}

base::string16 CreditCardEditorViewController::GetSheetTitle() {
  if (!credit_card_to_edit_)
    return l10n_util::GetStringUTF16(IDS_PAYMENTS_ADD_CARD);

  // Gets the completion message, or empty if nothing is missing from the card.
  base::string16 title = autofill::GetCompletionMessageForCard(
      autofill::GetCompletionStatusForCard(
          *credit_card_to_edit_, state()->GetApplicationLocale(),
          state()->GetPersonalDataManager()->GetProfiles()));
  return title.empty() ? l10n_util::GetStringUTF16(IDS_PAYMENTS_EDIT_CARD)
                       : title;
}

void CreditCardEditorViewController::ButtonPressed(views::Button* sender,
                                                   const ui::Event& event) {
  if (sender->tag() == add_billing_address_button_tag_) {
    dialog()->ShowShippingAddressEditor(
        BackNavigationType::kOneStep,
        /*on_edited=*/
        base::OnceClosure(),
        /*on_added=*/
        base::BindOnce(
            &CreditCardEditorViewController::AddAndSelectNewBillingAddress,
            base::Unretained(this)),
        /*profile=*/nullptr);
  } else {
    EditorViewController::ButtonPressed(sender, event);
  }
}

void CreditCardEditorViewController::AddAndSelectNewBillingAddress(
    const autofill::AutofillProfile& profile) {
  views::Combobox* address_combobox =
      static_cast<views::Combobox*>(dialog()->GetViewByID(kBillingAddressType));
  autofill::AddressComboboxModel* model =
      static_cast<autofill::AddressComboboxModel*>(address_combobox->model());
  int index = model->AddNewProfile(profile);
  address_combobox->SetSelectedIndex(index);
  // Force revalidation.
  address_combobox->OnBlur();
}

CreditCardEditorViewController::CreditCardValidationDelegate::
    CreditCardValidationDelegate(
        const EditorField& field,
        EditorViewController* controller,
        const std::vector<std::string>& supported_card_networks)
    : field_(field),
      controller_(controller),
      supported_card_networks_(supported_card_networks.begin(),
                               supported_card_networks.end()) {}
CreditCardEditorViewController::CreditCardValidationDelegate::
    ~CreditCardValidationDelegate() {}

bool CreditCardEditorViewController::CreditCardValidationDelegate::
    ValidateTextfield(views::Textfield* textfield) {
  return ValidateValue(textfield->text());
}

bool CreditCardEditorViewController::CreditCardValidationDelegate::
    ValidateCombobox(views::Combobox* combobox) {
  // The billing address ID is the selected item identifier and not the combobox
  // value itself.
  if (field_.type == kBillingAddressType) {
    // TODO(crbug.com/718905) Find a way to deal with existing incomplete
    // addresses when choosing them as billing addresses.
    autofill::AddressComboboxModel* model =
        static_cast<autofill::AddressComboboxModel*>(combobox->model());
    return !model->GetItemIdentifierAt(combobox->selected_index()).empty();
  }
  return ValidateValue(combobox->GetTextForRow(combobox->selected_index()));
}

bool CreditCardEditorViewController::CreditCardValidationDelegate::
    ValidateValue(const base::string16& value) {
  if (!value.empty()) {
    base::string16 error_message;
    bool is_valid =
        field_.type == autofill::CREDIT_CARD_NUMBER
            ? autofill::IsValidCreditCardNumberForBasicCardNetworks(
                  value, supported_card_networks_, &error_message)
            : autofill::IsValidForType(value, field_.type, &error_message);
    controller_->DisplayErrorMessageForField(field_, error_message);
    return is_valid;
  }

  bool is_required_valid = !field_.required;
  const base::string16 displayed_message =
      is_required_valid ? base::ASCIIToUTF16("")
                        : l10n_util::GetStringUTF16(
                              IDS_PAYMENTS_FIELD_REQUIRED_VALIDATION_MESSAGE);
  controller_->DisplayErrorMessageForField(field_, displayed_message);
  return is_required_valid;
}

bool CreditCardEditorViewController::GetSheetId(DialogViewID* sheet_id) {
  *sheet_id = DialogViewID::CREDIT_CARD_EDITOR_SHEET;
  return true;
}

}  // namespace payments
