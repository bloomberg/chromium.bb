// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/autofill/autofill_dialog_controller_impl.h"

#include <string>

#include "base/bind.h"
#include "base/logging.h"
#include "base/prefs/pref_service.h"
#include "base/string_number_conversions.h"
#include "base/string_util.h"
#include "base/strings/string_split.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/autofill/autofill_country.h"
#include "chrome/browser/autofill/autofill_manager.h"
#include "chrome/browser/autofill/autofill_type.h"
#include "chrome/browser/autofill/personal_data_manager.h"
#include "chrome/browser/autofill/personal_data_manager_factory.h"
#include "chrome/browser/autofill/risk/fingerprint.h"
#include "chrome/browser/autofill/risk/proto/fingerprint.pb.h"
#include "chrome/browser/autofill/validation.h"
#include "chrome/browser/autofill/wallet/full_wallet.h"
#include "chrome/browser/autofill/wallet/wallet_items.h"
#include "chrome/browser/autofill/wallet/wallet_service_url.h"
#include "chrome/browser/extensions/shell_window_registry.h"
#include "chrome/browser/prefs/pref_registry_syncable.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill/autofill_dialog_view.h"
#include "chrome/browser/ui/autofill/data_model_wrapper.h"
#include "chrome/browser/ui/base_window.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/extensions/native_app_window.h"
#include "chrome/browser/ui/extensions/shell_window.h"
#include "chrome/common/form_data.h"
#include "chrome/common/pref_names.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "content/public/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/webkit_resources.h"
#include "net/base/cert_status_flags.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/skbitmap_operations.h"

namespace autofill {

namespace {

const bool kPayWithoutWalletDefault = false;

// Returns true if |input| should be shown when |field| has been requested.
bool InputTypeMatchesFieldType(const DetailInput& input,
                               const AutofillField& field) {
  // If any credit card expiration info is asked for, show both month and year
  // inputs.
  if (field.type() == CREDIT_CARD_EXP_4_DIGIT_YEAR ||
      field.type() == CREDIT_CARD_EXP_2_DIGIT_YEAR ||
      field.type() == CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR ||
      field.type() == CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR ||
      field.type() == CREDIT_CARD_EXP_MONTH) {
    return input.type == CREDIT_CARD_EXP_4_DIGIT_YEAR ||
           input.type == CREDIT_CARD_EXP_MONTH;
  }

  if (field.type() == CREDIT_CARD_TYPE)
    return input.type == CREDIT_CARD_NUMBER;

  return input.type == field.type();
}

// Returns true if |input| should be used for a site-requested |field|.
bool DetailInputMatchesField(const DetailInput& input,
                             const AutofillField& field) {
  bool right_section = !input.section_suffix ||
      EndsWith(field.section(), input.section_suffix, false);
  return InputTypeMatchesFieldType(input, field) && right_section;
}

// Returns true if |input| should be used to fill a site-requested |field| which
// is notated with a "shipping" tag, for use when the user has decided to use
// the billing address as the shipping address.
bool DetailInputMatchesShippingField(const DetailInput& input,
                                     const AutofillField& field) {
  if (input.section_suffix &&
      std::string(input.section_suffix) == "billing") {
    return InputTypeMatchesFieldType(input, field);
  }

  if (field.type() == NAME_FULL)
    return input.type == CREDIT_CARD_NAME;

  return DetailInputMatchesField(input, field);
}

// Constructs |inputs| from template data.
void BuildInputs(const DetailInput* input_template,
                 size_t template_size,
                 DetailInputs* inputs) {
  for (size_t i = 0; i < template_size; ++i) {
    const DetailInput* input = &input_template[i];
    inputs->push_back(*input);
  }
}

// Uses |group| to fill in the |autofilled_value| for all inputs in |all_inputs|
// (an out-param).
void FillInputFromFormGroup(FormGroup* group, DetailInputs* inputs) {
  const std::string app_locale = AutofillCountry::ApplicationLocale();
  for (size_t j = 0; j < inputs->size(); ++j) {
    (*inputs)[j].autofilled_value =
        group->GetInfo((*inputs)[j].type, app_locale);
  }
}

// Initializes |form_group| from user-entered data.
void FillFormGroupFromOutputs(const DetailOutputMap& detail_outputs,
                              FormGroup* form_group) {
  for (DetailOutputMap::const_iterator iter = detail_outputs.begin();
       iter != detail_outputs.end(); ++iter) {
    if (!iter->second.empty())
      form_group->SetRawInfo(iter->first->type, iter->second);
  }
}

// Returns the containing window for the given |web_contents|.  The containing
// window might be a browser window for a Chrome tab, or it might be a shell
// window for a platform app.
BaseWindow* GetBaseWindowForWebContents(
    const content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  if (browser)
    return browser->window();

  gfx::NativeWindow native_window =
      web_contents->GetView()->GetTopLevelNativeWindow();
  ShellWindow* shell_window =
      extensions::ShellWindowRegistry::
          GetShellWindowForNativeWindowAnyProfile(native_window);
  return shell_window->GetBaseWindow();
}

}  // namespace

AutofillDialogController::~AutofillDialogController() {}

AutofillDialogControllerImpl::AutofillDialogControllerImpl(
    content::WebContents* contents,
    const FormData& form,
    const GURL& source_url,
    const content::SSLStatus& ssl_status,
    const AutofillMetrics& metric_logger,
    DialogType dialog_type,
    const base::Callback<void(const FormStructure*)>& callback)
    : profile_(Profile::FromBrowserContext(contents->GetBrowserContext())),
      contents_(contents),
      form_structure_(form, std::string()),
      source_url_(source_url),
      ssl_status_(ssl_status),
      callback_(callback),
      ALLOW_THIS_IN_INITIALIZER_LIST(
          account_chooser_model_(this, profile_->GetPrefs())),
      wallet_client_(profile_->GetRequestContext()),
      refresh_wallet_items_queued_(false),
      ALLOW_THIS_IN_INITIALIZER_LIST(suggested_email_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(suggested_cc_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(suggested_billing_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(suggested_cc_billing_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(suggested_shipping_(this)),
      section_showing_popup_(SECTION_BILLING),
      ALLOW_THIS_IN_INITIALIZER_LIST(weak_ptr_factory_(this)),
      metric_logger_(metric_logger),
      dialog_type_(dialog_type) {
  // TODO(estade): remove duplicates from |form|?
  content::NavigationEntry* entry = contents->GetController().GetActiveEntry();
  const GURL& active_url = entry ? entry->GetURL() : web_contents()->GetURL();
  invoked_from_same_origin_ = active_url.GetOrigin() == source_url_.GetOrigin();
}

AutofillDialogControllerImpl::~AutofillDialogControllerImpl() {
  if (popup_controller_)
    popup_controller_->Hide();
}

// static
void AutofillDialogControllerImpl::RegisterUserPrefs(
    PrefRegistrySyncable* registry) {
  registry->RegisterBooleanPref(prefs::kAutofillDialogPayWithoutWallet,
                                kPayWithoutWalletDefault,
                                PrefRegistrySyncable::SYNCABLE_PREF);
}

void AutofillDialogControllerImpl::Show() {
  dialog_shown_timestamp_ = base::Time::Now();

  bool has_types = false;
  bool has_sections = false;
  form_structure_.ParseFieldTypesFromAutocompleteAttributes(&has_types,
                                                            &has_sections);
  // Fail if the author didn't specify autocomplete types.
  if (!has_types) {
    callback_.Run(NULL);
    delete this;
    return;
  }

  const DetailInput kEmailInputs[] = {
    { 1, EMAIL_ADDRESS, IDS_AUTOFILL_DIALOG_PLACEHOLDER_EMAIL },
  };

  const DetailInput kCCInputs[] = {
    { 2, CREDIT_CARD_NUMBER, IDS_AUTOFILL_DIALOG_PLACEHOLDER_CARD_NUMBER },
    { 3, CREDIT_CARD_EXP_MONTH },
    { 3, CREDIT_CARD_EXP_4_DIGIT_YEAR },
    { 3, CREDIT_CARD_VERIFICATION_CODE, IDS_AUTOFILL_DIALOG_PLACEHOLDER_CVC },
    { 4, CREDIT_CARD_NAME, IDS_AUTOFILL_DIALOG_PLACEHOLDER_CARDHOLDER_NAME },
  };

  const DetailInput kBillingInputs[] = {
    { 5, ADDRESS_HOME_LINE1, IDS_AUTOFILL_DIALOG_PLACEHOLDER_ADDRESS_LINE_1,
      "billing" },
    { 6, ADDRESS_HOME_LINE2, IDS_AUTOFILL_DIALOG_PLACEHOLDER_ADDRESS_LINE_2,
      "billing" },
    { 7, ADDRESS_HOME_CITY, IDS_AUTOFILL_DIALOG_PLACEHOLDER_LOCALITY,
      "billing" },
    // TODO(estade): state is supposed to be a combobox.
    { 8, ADDRESS_HOME_STATE, IDS_AUTOFILL_FIELD_LABEL_STATE, "billing" },
    { 8, ADDRESS_HOME_ZIP, IDS_AUTOFILL_DIALOG_PLACEHOLDER_POSTAL_CODE,
      "billing", 0.5 },
    // TODO(estade): this should have a default based on the locale.
    { 9, ADDRESS_HOME_COUNTRY, 0, "billing" },
    { 10, PHONE_HOME_WHOLE_NUMBER,
      IDS_AUTOFILL_DIALOG_PLACEHOLDER_PHONE_NUMBER },
  };

  const DetailInput kShippingInputs[] = {
    { 11, NAME_FULL, IDS_AUTOFILL_DIALOG_PLACEHOLDER_ADDRESSEE_NAME,
      "shipping" },
    { 12, ADDRESS_HOME_LINE1, IDS_AUTOFILL_DIALOG_PLACEHOLDER_ADDRESS_LINE_1,
      "shipping" },
    { 13, ADDRESS_HOME_LINE2, IDS_AUTOFILL_DIALOG_PLACEHOLDER_ADDRESS_LINE_2,
      "shipping" },
    { 14, ADDRESS_HOME_CITY, IDS_AUTOFILL_DIALOG_PLACEHOLDER_LOCALITY,
      "shipping" },
    // TODO(estade): state is supposed to be a combobox.
    { 15, ADDRESS_HOME_STATE, IDS_AUTOFILL_FIELD_LABEL_STATE, "shipping" },
    { 15, ADDRESS_HOME_ZIP, IDS_AUTOFILL_DIALOG_PLACEHOLDER_POSTAL_CODE,
      "shipping", 0.5 },
  };

  BuildInputs(kEmailInputs,
              arraysize(kEmailInputs),
              &requested_email_fields_);

  BuildInputs(kCCInputs,
              arraysize(kCCInputs),
              &requested_cc_fields_);

  BuildInputs(kBillingInputs,
              arraysize(kBillingInputs),
              &requested_billing_fields_);

  BuildInputs(kCCInputs,
              arraysize(kCCInputs),
              &requested_cc_billing_fields_);
  BuildInputs(kBillingInputs,
              arraysize(kBillingInputs),
              &requested_cc_billing_fields_);

  BuildInputs(kShippingInputs,
              arraysize(kShippingInputs),
              &requested_shipping_fields_);

  GenerateSuggestionsModels();

  // TODO(estade): don't show the dialog if the site didn't specify the right
  // fields. First we must figure out what the "right" fields are.
  view_.reset(AutofillDialogView::Create(this));
  view_->Show();
  GetManager()->AddObserver(this);

  // Request sugar info after the view is showing to simplify code for now.
  if (IsPayingWithWallet())
    ScheduleRefreshWalletItems();
}

void AutofillDialogControllerImpl::Hide() {
  if (view_)
    view_->Hide();
}

void AutofillDialogControllerImpl::UpdateProgressBar(double value) {
  view_->UpdateProgressBar(value);
}

////////////////////////////////////////////////////////////////////////////////
// AutofillDialogController implementation.

string16 AutofillDialogControllerImpl::DialogTitle() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_TITLE);
}

string16 AutofillDialogControllerImpl::EditSuggestionText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_EDIT);
}

string16 AutofillDialogControllerImpl::UseBillingForShippingText() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_DIALOG_USE_BILLING_FOR_SHIPPING);
}

string16 AutofillDialogControllerImpl::CancelButtonText() const {
  return l10n_util::GetStringUTF16(IDS_CANCEL);
}

string16 AutofillDialogControllerImpl::ConfirmButtonText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SUBMIT_BUTTON);
}

string16 AutofillDialogControllerImpl::CancelSignInText() const {
  // TODO(abodenha): real strings and l10n.
  return ASCIIToUTF16("Don't sign in.");
}

string16 AutofillDialogControllerImpl::SaveLocallyText() const {
  return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SAVE_LOCALLY_CHECKBOX);
}

string16 AutofillDialogControllerImpl::ProgressBarText() const {
  return l10n_util::GetStringUTF16(
      IDS_AUTOFILL_DIALOG_AUTOCHECKOUT_PROGRESS_BAR);
}

DialogSignedInState AutofillDialogControllerImpl::SignedInState() const {
  if (!wallet_items_)
    return REQUIRES_RESPONSE;

  if (HasRequiredAction(wallet::GAIA_AUTH))
    return REQUIRES_SIGN_IN;

  if (HasRequiredAction(wallet::PASSIVE_GAIA_AUTH))
    return REQUIRES_PASSIVE_SIGN_IN;

  return SIGNED_IN;
}

string16 AutofillDialogControllerImpl::AccountChooserText() const {
  if (!IsPayingWithWallet())
    return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PAY_WITHOUT_WALLET);

  // TODO(dbeam): real strings and l10n.
  if (SignedInState() == SIGNED_IN)
    return ASCIIToUTF16("user@example.com");

  return ASCIIToUTF16("Sign in to use Google Wallet");
}

bool AutofillDialogControllerImpl::ShouldOfferToSaveInChrome() const {
  return !IsPayingWithWallet();
}

bool AutofillDialogControllerImpl::SectionIsActive(DialogSection section)
    const {
  if (IsPayingWithWallet())
    return section != SECTION_BILLING && section != SECTION_CC;

  return section != SECTION_CC_BILLING;
}

bool AutofillDialogControllerImpl::HasCompleteWallet() const {
  return wallet_items_.get() != NULL &&
         !wallet_items_->instruments().empty() &&
         !wallet_items_->addresses().empty();
}

const DetailInputs& AutofillDialogControllerImpl::RequestedFieldsForSection(
    DialogSection section) const {
  switch (section) {
    case SECTION_EMAIL:
      return requested_email_fields_;
    case SECTION_CC:
      return requested_cc_fields_;
    case SECTION_BILLING:
      return requested_billing_fields_;
    case SECTION_CC_BILLING:
      return requested_cc_billing_fields_;
    case SECTION_SHIPPING:
      return requested_shipping_fields_;
  }

  NOTREACHED();
  return requested_billing_fields_;
}

ui::ComboboxModel* AutofillDialogControllerImpl::ComboboxModelForAutofillType(
    AutofillFieldType type) {
  switch (type) {
    case CREDIT_CARD_EXP_MONTH:
      return &cc_exp_month_combobox_model_;

    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return &cc_exp_year_combobox_model_;

    case ADDRESS_HOME_COUNTRY:
      return &country_combobox_model_;

    default:
      return NULL;
  }
}

ui::MenuModel* AutofillDialogControllerImpl::MenuModelForSection(
    DialogSection section) {
  return SuggestionsMenuModelForSection(section);
}

ui::MenuModel* AutofillDialogControllerImpl::MenuModelForAccountChooser() {
  // When paying with wallet, but not signed in, there is no menu, just a
  // sign in link.
  if (IsPayingWithWallet() && SignedInState() != SIGNED_IN)
    return NULL;

  return &account_chooser_model_;
}

string16 AutofillDialogControllerImpl::LabelForSection(DialogSection section)
    const {
  switch (section) {
    case SECTION_EMAIL:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_EMAIL);
    case SECTION_CC:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_CC);
    case SECTION_BILLING:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_BILLING);
    case SECTION_CC_BILLING:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_CC_BILLING);
    case SECTION_SHIPPING:
      return l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECTION_SHIPPING);
    default:
      NOTREACHED();
      return string16();
  }
}

string16 AutofillDialogControllerImpl::SuggestionTextForSection(
    DialogSection section) {
  // When the user has clicked 'edit', don't show a suggestion (even though
  // there is a profile selected in the model).
  if (section_editing_state_[section])
    return string16();

  SuggestionsMenuModel* model = SuggestionsMenuModelForSection(section);
  std::string item_key = model->GetItemKeyForCheckedItem();
  if (item_key.empty())
    return string16();

  if (section == SECTION_EMAIL)
    return model->GetLabelAt(model->checked_item());

  scoped_ptr<DataModelWrapper> wrapper = CreateWrapper(section);
  return wrapper->GetDisplayText();
}

scoped_ptr<DataModelWrapper> AutofillDialogControllerImpl::CreateWrapper(
    DialogSection section) {
  SuggestionsMenuModel* model = SuggestionsMenuModelForSection(section);
  std::string item_key = model->GetItemKeyForCheckedItem();
  scoped_ptr<DataModelWrapper> wrapper;
  if (item_key.empty())
    return wrapper.Pass();

  if (IsPayingWithWallet()) {
    int index;
    bool success = base::StringToInt(item_key, &index);
    DCHECK(success);

    if (section == SECTION_CC_BILLING) {
      wrapper.reset(
          new WalletInstrumentWrapper(wallet_items_->instruments()[index]));
    } else {
      wrapper.reset(
          new WalletAddressWrapper(wallet_items_->addresses()[index]));
    }
  } else if (section == SECTION_CC) {
    CreditCard* card = GetManager()->GetCreditCardByGUID(item_key);
    DCHECK(card);
    wrapper.reset(new AutofillCreditCardWrapper(card));
  } else {
    // Calculate the variant by looking at how many items come from the same
    // FormGroup. TODO(estade): add a test for this.
    size_t variant = 0;
    for (int i = model->checked_item() - 1; i >= 0; --i) {
      if (model->GetItemKeyAt(i) == item_key)
        variant++;
      else
        break;
    }

    AutofillProfile* profile = GetManager()->GetProfileByGUID(item_key);
    DCHECK(profile);
    wrapper.reset(new AutofillDataModelWrapper(profile, variant));
  }

  return wrapper.Pass();
}

gfx::Image AutofillDialogControllerImpl::SuggestionIconForSection(
    DialogSection section) {
  scoped_ptr<DataModelWrapper> model = CreateWrapper(section);
  if (!model.get())
    return gfx::Image();

  return model->GetIcon();
}

void AutofillDialogControllerImpl::EditClickedForSection(
    DialogSection section) {
  SuggestionsMenuModel* model = SuggestionsMenuModelForSection(section);
  DetailInputs* inputs = MutableRequestedFieldsForSection(section);

  if (section == SECTION_EMAIL) {
    // TODO(estade): shouldn't need to make this check.
    if (inputs->empty())
      return;

    (*inputs)[0].autofilled_value = model->GetLabelAt(model->checked_item());
  } else {
    scoped_ptr<DataModelWrapper> model = CreateWrapper(section);
    model->FillInputs(inputs);
  }

  section_editing_state_[section] = true;
  view_->UpdateSection(section);
}

gfx::Image AutofillDialogControllerImpl::IconForField(
    AutofillFieldType type, const string16& user_input) const {
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  if (type == CREDIT_CARD_VERIFICATION_CODE)
    return rb.GetImageNamed(IDR_CREDIT_CARD_CVC_HINT);

  // For the credit card, we show a few grayscale images, and possibly one
  // color image if |user_input| is a valid card number.
  if (type == CREDIT_CARD_NUMBER) {
    const int card_idrs[] = {
      IDR_AUTOFILL_CC_VISA,
      IDR_AUTOFILL_CC_MASTERCARD,
      IDR_AUTOFILL_CC_AMEX,
      IDR_AUTOFILL_CC_DISCOVER
    };
    const int number_of_cards = arraysize(card_idrs);
    // The number of pixels between card icons.
    const int kCardPadding = 2;

    gfx::ImageSkia some_card = *rb.GetImageSkiaNamed(card_idrs[0]);
    const int card_width = some_card.width();
    gfx::Canvas canvas(
        gfx::Size((card_width + kCardPadding) * number_of_cards - kCardPadding,
                  some_card.height()),
        ui::SCALE_FACTOR_100P,
        true);
    CreditCard card;
    card.SetRawInfo(CREDIT_CARD_NUMBER, user_input);

    for (int i = 0; i < number_of_cards; ++i) {
      int idr = card_idrs[i];
      gfx::ImageSkia card_image = *rb.GetImageSkiaNamed(idr);
      if (card.IconResourceId() != idr) {
        color_utils::HSL shift = {-1, 0, 0.8};
        SkBitmap disabled_bitmap =
            SkBitmapOperations::CreateHSLShiftedBitmap(*card_image.bitmap(),
                                                       shift);
        card_image = gfx::ImageSkia::CreateFrom1xBitmap(disabled_bitmap);
      }

      canvas.DrawImageInt(card_image, i * (card_width + kCardPadding), 0);
    }

    gfx::ImageSkia skia(canvas.ExtractImageRep());
    return gfx::Image(skia);
  }

  return gfx::Image();
}

bool AutofillDialogControllerImpl::InputIsValid(AutofillFieldType type,
                                                const string16& value) {
  switch (type) {
    case EMAIL_ADDRESS:
      return IsValidEmailAddress(value);

    case CREDIT_CARD_NUMBER:
      return autofill::IsValidCreditCardNumber(value);
    case CREDIT_CARD_NAME:
      break;
    case CREDIT_CARD_EXP_MONTH:
    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      break;
    case CREDIT_CARD_VERIFICATION_CODE:
      return autofill::IsValidCreditCardSecurityCode(value);

    case ADDRESS_HOME_LINE1:
      break;
    case ADDRESS_HOME_LINE2:
      return true;  // Line 2 is optional - always valid.
    case ADDRESS_HOME_CITY:
    case ADDRESS_HOME_STATE:
    case ADDRESS_HOME_ZIP:
    case ADDRESS_HOME_COUNTRY:
      break;

    case NAME_FULL:  // Used for shipping.
      break;

    case PHONE_HOME_WHOLE_NUMBER:  // Used in billing section.
      // TODO(dbeam): validate with libphonenumber.
      break;

    default:
      NOTREACHED();  // Trying to validate unknown field.
      break;
  }

  return !value.empty();
}

void AutofillDialogControllerImpl::UserEditedOrActivatedInput(
    const DetailInput* input,
    DialogSection section,
    gfx::NativeView parent_view,
    const gfx::Rect& content_bounds,
    const string16& field_contents,
    bool was_edit) {
  // If the field is edited down to empty, don't show a popup.
  if (was_edit && field_contents.empty()) {
    HidePopup();
    return;
  }

  // If the user clicks while the popup is already showing, be sure to hide
  // it.
  if (!was_edit && popup_controller_) {
    HidePopup();
    return;
  }

  std::vector<string16> popup_values, popup_labels, popup_icons;
  if (section == SECTION_CC) {
    GetManager()->GetCreditCardSuggestions(input->type,
                                           field_contents,
                                           &popup_values,
                                           &popup_labels,
                                           &popup_icons,
                                           &popup_guids_);
  } else {
    // TODO(estade): add field types from email section?
    const DetailInputs& inputs = RequestedFieldsForSection(section);
    std::vector<AutofillFieldType> field_types;
    field_types.reserve(inputs.size());
    for (DetailInputs::const_iterator iter = inputs.begin();
         iter != inputs.end(); ++iter) {
      field_types.push_back(iter->type);
    }
    GetManager()->GetProfileSuggestions(input->type,
                                        field_contents,
                                        false,
                                        field_types,
                                        &popup_values,
                                        &popup_labels,
                                        &popup_icons,
                                        &popup_guids_);
  }

  if (popup_values.empty())
    return;

  // TODO(estade): do we need separators and control rows like 'Clear
  // Form'?
  std::vector<int> popup_ids;
  for (size_t i = 0; i < popup_guids_.size(); ++i) {
    popup_ids.push_back(i);
  }

  popup_controller_ = AutofillPopupControllerImpl::GetOrCreate(
      popup_controller_, this, parent_view, content_bounds);
  popup_controller_->Show(popup_values,
                          popup_labels,
                          popup_icons,
                          popup_ids);
  section_showing_popup_ = section;
}

void AutofillDialogControllerImpl::FocusMoved() {
  HidePopup();
}

void AutofillDialogControllerImpl::ViewClosed(DialogAction action) {
  GetManager()->RemoveObserver(this);

  if (action == ACTION_SUBMIT) {
    FillOutputForSection(SECTION_EMAIL);
    FillOutputForSection(SECTION_CC);
    FillOutputForSection(SECTION_BILLING);
    if (view_->UseBillingForShipping()) {
      FillOutputForSectionWithComparator(
          SECTION_BILLING,
          base::Bind(DetailInputMatchesShippingField));
      FillOutputForSectionWithComparator(
          SECTION_CC,
          base::Bind(DetailInputMatchesShippingField));
    } else {
      FillOutputForSection(SECTION_SHIPPING);
    }

    // On a successful submit, save the payment type for next time. If there
    // was a Wallet server error, try to pay with Wallet again next time.
    // Reset the view so that updates to the pref aren't processed.
    view_.reset();
    bool pay_without_wallet = !IsPayingWithWallet() &&
        !account_chooser_model_.had_wallet_error();
    profile_->GetPrefs()->SetBoolean(prefs::kAutofillDialogPayWithoutWallet,
                                     pay_without_wallet);

    callback_.Run(&form_structure_);
  } else {
    callback_.Run(NULL);
  }

  AutofillMetrics::DialogDismissalAction metric =
      action == ACTION_SUBMIT ?
          AutofillMetrics::DIALOG_ACCEPTED :
          AutofillMetrics::DIALOG_CANCELED;
  metric_logger_.LogRequestAutocompleteUiDuration(
      base::Time::Now() - dialog_shown_timestamp_, dialog_type_, metric);

  delete this;
}

std::vector<DialogNotification>
    AutofillDialogControllerImpl::CurrentNotifications() const {
  std::vector<DialogNotification> notifications;

  if (IsPayingWithWallet()) {
    // TODO(dbeam): what about REQUIRES_PASSIVE_SIGN_IN?
    if (SignedInState() == SIGNED_IN) {
      // On first run with a complete wallet profile, show a notification
      // explaining where this data came from.
      if (IsFirstRun() && HasCompleteWallet()) {
        notifications.push_back(
            DialogNotification(
                DialogNotification::EXPLANATORY_MESSAGE,
                l10n_util::GetStringUTF16(
                    IDS_AUTOFILL_DIALOG_DETAILS_FROM_WALLET)));
      } else {
        notifications.push_back(
            DialogNotification(
                DialogNotification::WALLET_PROMO,
                l10n_util::GetStringUTF16(
                    IDS_AUTOFILL_DIALOG_SAVE_DETAILS_IN_WALLET)));
      }
    } else if (IsFirstRun()) {
      // If the user is not signed in, show an upsell notification on first run.
      notifications.push_back(
          DialogNotification(
              DialogNotification::WALLET_PROMO,
              l10n_util::GetStringUTF16(
                  IDS_AUTOFILL_DIALOG_SIGN_IN_AND_SAVE_DETAILS)));
    }

    if (HasRequiredAction(wallet::VERIFY_CVV)) {
      notifications.push_back(
          DialogNotification(
              DialogNotification::REQUIRED_ACTION,
              l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_VERIFY_CVV)));
    }
  }

  if (RequestingCreditCardInfo() && !TransmissionWillBeSecure()) {
    notifications.push_back(
        DialogNotification(
            DialogNotification::SECURITY_WARNING,
            l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_SECURITY_WARNING)));
  }

  if (!invoked_from_same_origin_) {
    notifications.push_back(
        DialogNotification(
            DialogNotification::SECURITY_WARNING,
            l10n_util::GetStringFUTF16(
                IDS_AUTOFILL_DIALOG_SITE_WARNING,
                UTF8ToUTF16(source_url_.host()))));
  }

  if (account_chooser_model_.had_wallet_error()) {
    // TODO(dbeam): pass along the Wallet error or remove from the translation.
    // TODO(dbeam): figure out a way to dismiss this error after a while.
    notifications.push_back(DialogNotification(
        DialogNotification::WALLET_ERROR,
        l10n_util::GetStringFUTF16(
            IDS_AUTOFILL_DIALOG_COMPLETE_WITHOUT_WALLET,
            ASCIIToUTF16("Oops, [Wallet-Error]."))));
  }

  return notifications;
}

void AutofillDialogControllerImpl::StartSignInFlow() {
  DCHECK(registrar_.IsEmpty());

  content::Source<content::NavigationController> source(&view_->ShowSignIn());
  registrar_.Add(this, content::NOTIFICATION_NAV_ENTRY_COMMITTED, source);
}

void AutofillDialogControllerImpl::EndSignInFlow() {
  DCHECK(!registrar_.IsEmpty());
  registrar_.RemoveAll();
  view_->HideSignIn();
}

Profile* AutofillDialogControllerImpl::profile() {
  return profile_;
}

content::WebContents* AutofillDialogControllerImpl::web_contents() {
  return contents_;
}

////////////////////////////////////////////////////////////////////////////////
// AutofillPopupDelegate implementation.

void AutofillDialogControllerImpl::OnPopupShown(
    content::KeyboardListener* listener) {}

void AutofillDialogControllerImpl::OnPopupHidden(
    content::KeyboardListener* listener) {}

void AutofillDialogControllerImpl::DidSelectSuggestion(int identifier) {
  // TODO(estade): implement.
}

void AutofillDialogControllerImpl::DidAcceptSuggestion(const string16& value,
                                                       int identifier) {
  const PersonalDataManager::GUIDPair& pair = popup_guids_[identifier];

  FormGroup* form_group = section_showing_popup_ == SECTION_CC ?
      static_cast<FormGroup*>(GetManager()->GetCreditCardByGUID(pair.first)) :
      // TODO(estade): need to use the variant, |pair.second|.
      static_cast<FormGroup*>(GetManager()->GetProfileByGUID(pair.first));
  DCHECK(form_group);

  FillInputFromFormGroup(
      form_group,
      MutableRequestedFieldsForSection(section_showing_popup_));
  view_->UpdateSection(section_showing_popup_);

  // TODO(estade): not sure why it's necessary to do this explicitly.
  HidePopup();
}

void AutofillDialogControllerImpl::RemoveSuggestion(const string16& value,
                                                    int identifier) {
  // TODO(estade): implement.
}

void AutofillDialogControllerImpl::ClearPreviewedForm() {
  // TODO(estade): implement.
}

////////////////////////////////////////////////////////////////////////////////
// content::NotificationObserver implementation.

void AutofillDialogControllerImpl::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, content::NOTIFICATION_NAV_ENTRY_COMMITTED);
  content::LoadCommittedDetails* load_details =
      content::Details<content::LoadCommittedDetails>(details).ptr();
  if (wallet::IsSignInContinueUrl(load_details->entry->GetVirtualURL())) {
    EndSignInFlow();
    if (IsPayingWithWallet())
      ScheduleRefreshWalletItems();
  }
}

////////////////////////////////////////////////////////////////////////////////
// SuggestionsMenuModelDelegate implementation.

void AutofillDialogControllerImpl::SuggestionItemSelected(
    const SuggestionsMenuModel& model) {
  DialogSection section = SectionForSuggestionsMenuModel(model);
  section_editing_state_[section] = false;
  view_->UpdateSection(section);
}

////////////////////////////////////////////////////////////////////////////////
// wallet::WalletClientObserver implementation.

void AutofillDialogControllerImpl::OnDidAcceptLegalDocuments() {
  NOTIMPLEMENTED();
}

void AutofillDialogControllerImpl::OnDidAuthenticateInstrument(bool success) {
  NOTIMPLEMENTED() << " authenticated=" << success;
}

void AutofillDialogControllerImpl::OnDidGetFullWallet(
    scoped_ptr<wallet::FullWallet> full_wallet) {
  NOTIMPLEMENTED();
  WalletRequestCompleted(true);
}

void AutofillDialogControllerImpl::OnDidGetWalletItems(
    scoped_ptr<wallet::WalletItems> wallet_items) {
  bool items_changed = !wallet_items_ || *wallet_items != *wallet_items_;
  wallet_items_ = wallet_items.Pass();
  WalletRequestCompleted(true);

  if (items_changed) {
    GenerateSuggestionsModels();
    view_->ModelChanged();
    view_->UpdateAccountChooser();
    view_->UpdateNotificationArea();
  }
}

void AutofillDialogControllerImpl::OnDidSaveAddress(
    const std::string& address_id,
    const std::vector<wallet::RequiredAction>& required_actions) {
  NOTIMPLEMENTED() << " address_id=" << address_id
                   << ", required_actions=" << !required_actions.empty();
  WalletRequestCompleted(true);
}

void AutofillDialogControllerImpl::OnDidSaveInstrument(
    const std::string& instrument_id,
    const std::vector<wallet::RequiredAction>& required_actions) {
  NOTIMPLEMENTED() << " instrument_id=" << instrument_id
                   << ", required_actions=" << !required_actions.empty();
  WalletRequestCompleted(true);
}

void AutofillDialogControllerImpl::OnDidSaveInstrumentAndAddress(
    const std::string& instrument_id,
    const std::string& address_id,
    const std::vector<wallet::RequiredAction>& required_actions) {
  NOTIMPLEMENTED() << " instrument_id=" << instrument_id
                   << ", address_id=" << address_id
                   << ", required_actions=" << !required_actions.empty();
  WalletRequestCompleted(true);
}

void AutofillDialogControllerImpl::OnDidSendAutocheckoutStatus() {
  NOTIMPLEMENTED();
  WalletRequestCompleted(true);
}

void AutofillDialogControllerImpl::OnDidUpdateInstrument(
    const std::string& instrument_id,
    const std::vector<wallet::RequiredAction>& required_actions) {
  NOTIMPLEMENTED() << " instrument_id=" << instrument_id
                   << ", required_actions=" << !required_actions.empty();
}

void AutofillDialogControllerImpl::OnWalletError() {
  WalletRequestCompleted(false);
}

void AutofillDialogControllerImpl::OnMalformedResponse() {
  WalletRequestCompleted(false);
}

void AutofillDialogControllerImpl::OnNetworkError(int response_code) {
  WalletRequestCompleted(false);
}

////////////////////////////////////////////////////////////////////////////////
// PersonalDataManagerObserver implementation.

void AutofillDialogControllerImpl::OnPersonalDataChanged() {
  HidePopup();
  GenerateSuggestionsModels();
  view_->ModelChanged();
}

void AutofillDialogControllerImpl::AccountChoiceChanged() {
  if (!view_)
    return;

  GenerateSuggestionsModels();
  view_->ModelChanged();
  view_->UpdateAccountChooser();
  view_->UpdateNotificationArea();

  if (IsPayingWithWallet() && !wallet_items_)
    ScheduleRefreshWalletItems();
}

////////////////////////////////////////////////////////////////////////////////

bool AutofillDialogControllerImpl::HandleKeyPressEventInInput(
    const content::NativeWebKeyboardEvent& event) {
  if (popup_controller_)
    return popup_controller_->HandleKeyPressEvent(event);

  return false;
}

bool AutofillDialogControllerImpl::RequestingCreditCardInfo() const {
  DCHECK_GT(form_structure_.field_count(), 0U);

  for (size_t i = 0; i < form_structure_.field_count(); ++i) {
    if (AutofillType(form_structure_.field(i)->type()).group() ==
        AutofillType::CREDIT_CARD) {
      return true;
    }
  }

  return false;
}

bool AutofillDialogControllerImpl::TransmissionWillBeSecure() const {
  return source_url_.SchemeIs(chrome::kHttpsScheme) &&
         !net::IsCertStatusError(ssl_status_.cert_status) &&
         !net::IsCertStatusMinorError(ssl_status_.cert_status);
}

bool AutofillDialogControllerImpl::HasRequiredAction(
    wallet::RequiredAction action) const {
  if (!wallet_items_)
    return false;

  const std::vector<wallet::RequiredAction>& actions =
      wallet_items_->required_actions();
  return std::find(actions.begin(), actions.end(), action) != actions.end();
}

bool AutofillDialogControllerImpl::IsPayingWithWallet() const {
  return account_chooser_model_.WalletIsSelected();
}

void AutofillDialogControllerImpl::ScheduleRefreshWalletItems() {
  DCHECK(IsPayingWithWallet());

  if (wallet_client_.HasRequestInProgress()) {
    refresh_wallet_items_queued_ = true;
    return;
  }

  wallet_client_.GetWalletItems(source_url_, weak_ptr_factory_.GetWeakPtr());
  refresh_wallet_items_queued_ = false;
}

void AutofillDialogControllerImpl::WalletRequestCompleted(bool success) {
  if (!success) {
    wallet_items_.reset();
    GenerateSuggestionsModels();
    view_->ModelChanged();
    view_->UpdateNotificationArea();
    account_chooser_model_.SetHadWalletError();
    return;
  }

  if (refresh_wallet_items_queued_)
    ScheduleRefreshWalletItems();
}

bool AutofillDialogControllerImpl::IsFirstRun() const {
  PrefService* prefs = profile_->GetPrefs();
  return !prefs->HasPrefPath(prefs::kAutofillDialogPayWithoutWallet);
}

void AutofillDialogControllerImpl::GenerateSuggestionsModels() {
  suggested_email_.Reset();
  suggested_cc_.Reset();
  suggested_billing_.Reset();
  suggested_cc_billing_.Reset();
  suggested_shipping_.Reset();

  if (IsPayingWithWallet()) {
    if (wallet_items_.get()) {
      // TODO(estade): seems we need to hardcode the email address.

      const std::vector<wallet::Address*>& addresses =
          wallet_items_->addresses();
      for (size_t i = 0; i < addresses.size(); ++i) {
        suggested_shipping_.AddKeyedItem(base::IntToString(i),
                                         addresses[i]->DisplayName());
      }

      const std::vector<wallet::WalletItems::MaskedInstrument*>& instruments =
          wallet_items_->instruments();
      for (size_t i = 0; i < instruments.size(); ++i) {
        suggested_cc_billing_.AddKeyedItem(
            base::IntToString(i),
            instruments[i]->descriptive_name());
      }
    }

    suggested_cc_billing_.AddKeyedItem(
        std::string(),
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADD_BILLING_DETAILS));
  } else {
    PersonalDataManager* manager = GetManager();
    const std::vector<CreditCard*>& cards = manager->credit_cards();
    for (size_t i = 0; i < cards.size(); ++i) {
      suggested_cc_.AddKeyedItem(cards[i]->guid(), cards[i]->Label());
    }

    const std::vector<AutofillProfile*>& profiles = manager->GetProfiles();
    const std::string app_locale = AutofillCountry::ApplicationLocale();
    for (size_t i = 0; i < profiles.size(); ++i) {
      if (!IsCompleteProfile(*profiles[i]))
        continue;

      // Add all email addresses.
      std::vector<string16> values;
      profiles[i]->GetMultiInfo(EMAIL_ADDRESS, app_locale, &values);
      for (size_t j = 0; j < values.size(); ++j) {
        if (!values[j].empty())
          suggested_email_.AddKeyedItem(profiles[i]->guid(), values[j]);
      }

      // Don't add variants for addresses: the email variants are handled above,
      // name is part of credit card and we'll just ignore phone number
      // variants.
      suggested_billing_.AddKeyedItem(profiles[i]->guid(),
                                      profiles[i]->Label());
      suggested_shipping_.AddKeyedItem(profiles[i]->guid(),
                                       profiles[i]->Label());
    }

    suggested_cc_.AddKeyedItem(
        std::string(),
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADD_CREDIT_CARD));
    suggested_billing_.AddKeyedItem(
        std::string(),
        l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADD_BILLING_ADDRESS));
  }

  suggested_email_.AddKeyedItem(
      std::string(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADD_EMAIL_ADDRESS));
  suggested_shipping_.AddKeyedItem(
      std::string(),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_ADD_SHIPPING_ADDRESS));
}

bool AutofillDialogControllerImpl::IsCompleteProfile(
    const AutofillProfile& profile) {
  const std::string app_locale = AutofillCountry::ApplicationLocale();
  for (size_t i = 0; i < requested_shipping_fields_.size(); ++i) {
    if (profile.GetInfo(requested_shipping_fields_[i].type,
                        app_locale).empty()) {
      return false;
    }
  }

  return true;
}

void AutofillDialogControllerImpl::FillOutputForSectionWithComparator(
    DialogSection section,
    const InputFieldComparator& compare) {
  if (!SectionIsActive(section))
    return;

  SuggestionsMenuModel* model = SuggestionsMenuModelForSection(section);
  std::string item_key = model->GetItemKeyForCheckedItem();
  if (!item_key.empty() && !section_editing_state_[section]) {
    scoped_ptr<DataModelWrapper> model = CreateWrapper(section);
    // Only fill in data that is associated with this section.
    const DetailInputs& inputs = RequestedFieldsForSection(section);
    model->FillFormStructure(inputs, compare, &form_structure_);

    // CVC needs special-casing because the CreditCard class doesn't store
    // or handle them.
    if (section == SECTION_CC)
      SetCvcResult(view_->GetCvc());
  } else {
    // The user manually input data.
    PersonalDataManager* manager = GetManager();
    DetailOutputMap output;
    view_->GetUserInput(section, &output);

    // Save the info as new or edited data, then fill it into |form_structure_|.
    if (section == SECTION_CC) {
      CreditCard card;
      FillFormGroupFromOutputs(output, &card);
      if (view_->SaveDetailsLocally())
        manager->SaveImportedCreditCard(card);
      FillFormStructureForSection(card, 0, section, compare);

      // Again, CVC needs special-casing. Fill it in directly from |output|.
      for (DetailOutputMap::iterator iter = output.begin();
           iter != output.end(); ++iter) {
        if (iter->first->type == CREDIT_CARD_VERIFICATION_CODE) {
          SetCvcResult(iter->second);
          break;
        }
      }
    } else {
      AutofillProfile profile;
      FillFormGroupFromOutputs(output, &profile);
      // TODO(estade): we should probably edit the existing profile in the cases
      // where the input data is based on an existing profile (user clicked
      // "Edit" or autofill popup filled in the form).
      if (view_->SaveDetailsLocally())
        manager->SaveImportedProfile(profile);
      FillFormStructureForSection(profile, 0, section, compare);
    }
  }
}

void AutofillDialogControllerImpl::FillOutputForSection(DialogSection section) {
  FillOutputForSectionWithComparator(section,
                                     base::Bind(DetailInputMatchesField));
}

void AutofillDialogControllerImpl::FillFormStructureForSection(
    const FormGroup& form_group,
    size_t variant,
    DialogSection section,
    const InputFieldComparator& compare) {
  for (size_t i = 0; i < form_structure_.field_count(); ++i) {
    AutofillField* field = form_structure_.field(i);
    // Only fill in data that is associated with this section.
    const DetailInputs& inputs = RequestedFieldsForSection(section);
    for (size_t j = 0; j < inputs.size(); ++j) {
      if (compare.Run(inputs[j], *field)) {
        form_group.FillFormField(*field, variant, field);
        break;
      }
    }
  }
}

void AutofillDialogControllerImpl::SetCvcResult(const string16& cvc) {
  for (size_t i = 0; i < form_structure_.field_count(); ++i) {
    AutofillField* field = form_structure_.field(i);
    if (field->type() == CREDIT_CARD_VERIFICATION_CODE) {
      field->value = cvc;
      break;
    }
  }
}

SuggestionsMenuModel* AutofillDialogControllerImpl::
    SuggestionsMenuModelForSection(DialogSection section) {
  switch (section) {
    case SECTION_EMAIL:
      return &suggested_email_;
    case SECTION_CC:
      return &suggested_cc_;
    case SECTION_BILLING:
      return &suggested_billing_;
    case SECTION_SHIPPING:
      return &suggested_shipping_;
    case SECTION_CC_BILLING:
      return &suggested_cc_billing_;
  }

  NOTREACHED();
  return NULL;
}

DialogSection AutofillDialogControllerImpl::SectionForSuggestionsMenuModel(
    const SuggestionsMenuModel& model) {
  if (&model == &suggested_email_)
    return SECTION_EMAIL;

  if (&model == &suggested_cc_)
    return SECTION_CC;

  if (&model == &suggested_billing_)
    return SECTION_BILLING;

  if (&model == &suggested_cc_billing_)
    return SECTION_CC_BILLING;

  DCHECK_EQ(&model, &suggested_shipping_);
  return SECTION_SHIPPING;
}

PersonalDataManager* AutofillDialogControllerImpl::GetManager() {
  return PersonalDataManagerFactory::GetForProfile(profile_);
}

DetailInputs* AutofillDialogControllerImpl::MutableRequestedFieldsForSection(
    DialogSection section) {
  return const_cast<DetailInputs*>(&RequestedFieldsForSection(section));
}

void AutofillDialogControllerImpl::HidePopup() {
  if (popup_controller_)
    popup_controller_->Hide();
}

void AutofillDialogControllerImpl::LoadRiskFingerprintData() {
  // TODO(dbeam): Add a CHECK or otherwise strong guarantee that the ToS have
  // been accepted prior to calling into this method.  Also, ensure that the UI
  // contains a clear indication to the user as to what data will be collected.
  // Until then, this code should not be called.

  int64 gaia_id = 0;
  bool success =
      base::StringToInt64(wallet_items_->obfuscated_gaia_id(), &gaia_id);
  DCHECK(success);

  gfx::Rect window_bounds =
      GetBaseWindowForWebContents(web_contents())->GetBounds();

  risk::GetFingerprint(
      gaia_id, window_bounds, *web_contents(), *profile_->GetPrefs(),
      base::Bind(&AutofillDialogControllerImpl::OnDidLoadRiskFingerprintData,
                 weak_ptr_factory_.GetWeakPtr()));
}

void AutofillDialogControllerImpl::OnDidLoadRiskFingerprintData(
    scoped_ptr<risk::Fingerprint> fingerprint) {
  NOTIMPLEMENTED();
}

}  // namespace autofill
