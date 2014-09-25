// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/wallet/wallet_items.h"

#include <limits>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/content/browser/wallet/gaia_account.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/credit_card.h"
#include "grit/components_scaled_resources.h"
#include "grit/components_strings.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"
#include "url/gurl.h"

namespace autofill {
namespace wallet {

namespace {

const char kLegalDocumentUrl[] =
    "https://wallet.google.com/legaldocument?docId=";
const char kPrivacyNoticeUrl[] = "https://wallet.google.com/files/privacy.html";

// TODO(estade): move to base/.
template<class T>
bool VectorsAreEqual(const std::vector<T*>& a, const std::vector<T*>& b) {
  if (a.size() != b.size())
    return false;

  for (size_t i = 0; i < a.size(); ++i) {
    if (*a[i] != *b[i])
      return false;
  }

  return true;
}

WalletItems::MaskedInstrument::Type
    TypeFromString(const std::string& type_string) {
  if (type_string == "VISA")
    return WalletItems::MaskedInstrument::VISA;
  if (type_string == "MASTER_CARD")
    return WalletItems::MaskedInstrument::MASTER_CARD;
  if (type_string == "AMEX")
    return WalletItems::MaskedInstrument::AMEX;
  if (type_string == "DISCOVER")
    return WalletItems::MaskedInstrument::DISCOVER;
  if (type_string == "SOLO")
    return WalletItems::MaskedInstrument::SOLO;
  if (type_string == "MAESTRO")
    return WalletItems::MaskedInstrument::MAESTRO;
  if (type_string == "SWITCH")
    return WalletItems::MaskedInstrument::SWITCH;
  return WalletItems::MaskedInstrument::UNKNOWN;
}

WalletItems::MaskedInstrument::Status
    StatusFromString(const std::string& status_string) {
  if (status_string == "AMEX_NOT_SUPPORTED")
    return WalletItems::MaskedInstrument::AMEX_NOT_SUPPORTED;
  if (status_string == "PENDING")
    return WalletItems::MaskedInstrument::PENDING;
  if (status_string == "VALID")
    return WalletItems::MaskedInstrument::VALID;
  if (status_string == "DECLINED")
    return WalletItems::MaskedInstrument::DECLINED;
  if (status_string == "DISABLED_FOR_THIS_MERCHANT")
    return WalletItems::MaskedInstrument::DISABLED_FOR_THIS_MERCHANT;
  if (status_string == "UNSUPPORTED_COUNTRY")
    return WalletItems::MaskedInstrument::UNSUPPORTED_COUNTRY;
  if (status_string == "EXPIRED")
    return WalletItems::MaskedInstrument::EXPIRED;
  if (status_string == "BILLING_INCOMPLETE")
    return WalletItems::MaskedInstrument::BILLING_INCOMPLETE;
  return WalletItems::MaskedInstrument::INAPPLICABLE;
}

base::string16 DisplayStringFromType(WalletItems::MaskedInstrument::Type type) {
  switch (type) {
    case WalletItems::MaskedInstrument::AMEX:
      return CreditCard::TypeForDisplay(kAmericanExpressCard);
    case WalletItems::MaskedInstrument::DISCOVER:
      return CreditCard::TypeForDisplay(kDiscoverCard);
    case WalletItems::MaskedInstrument::MASTER_CARD:
      return CreditCard::TypeForDisplay(kMasterCard);
    case WalletItems::MaskedInstrument::VISA:
      return CreditCard::TypeForDisplay(kVisaCard);
    default:
      return CreditCard::TypeForDisplay(kGenericCard);
  }
}

}  // anonymous namespace

WalletItems::MaskedInstrument::MaskedInstrument(
    const base::string16& descriptive_name,
    const WalletItems::MaskedInstrument::Type& type,
    const base::string16& last_four_digits,
    int expiration_month,
    int expiration_year,
    scoped_ptr<Address> address,
    const WalletItems::MaskedInstrument::Status& status,
    const std::string& object_id)
    : descriptive_name_(descriptive_name),
      type_(type),
      last_four_digits_(last_four_digits),
      expiration_month_(expiration_month),
      expiration_year_(expiration_year),
      address_(address.Pass()),
      status_(status),
      object_id_(object_id) {
  DCHECK(address_);
}

WalletItems::MaskedInstrument::~MaskedInstrument() {}

scoped_ptr<WalletItems::MaskedInstrument>
    WalletItems::MaskedInstrument::CreateMaskedInstrument(
    const base::DictionaryValue& dictionary) {
  std::string type_string;
  Type type;
  if (dictionary.GetString("type", &type_string)) {
    type = TypeFromString(type_string);
  } else {
    DLOG(ERROR) << "Response from Google Wallet missing card type";
    return scoped_ptr<MaskedInstrument>();
  }

  base::string16 last_four_digits;
  if (!dictionary.GetString("last_four_digits", &last_four_digits)) {
    DLOG(ERROR) << "Response from Google Wallet missing last four digits";
    return scoped_ptr<MaskedInstrument>();
  }

  std::string status_string;
  Status status;
  if (dictionary.GetString("status", &status_string)) {
    status = StatusFromString(status_string);
  } else {
    DLOG(ERROR) << "Response from Google Wallet missing status";
    return scoped_ptr<MaskedInstrument>();
  }

  std::string object_id;
  if (!dictionary.GetString("object_id", &object_id)) {
    DLOG(ERROR) << "Response from Google Wallet missing object id";
    return scoped_ptr<MaskedInstrument>();
  }

  const base::DictionaryValue* address_dict;
  if (!dictionary.GetDictionary("billing_address", &address_dict)) {
    DLOG(ERROR) << "Response from Google wallet missing address";
    return scoped_ptr<MaskedInstrument>();
  }
  scoped_ptr<Address> address = Address::CreateDisplayAddress(*address_dict);

  if (!address) {
    DLOG(ERROR) << "Response from Google wallet contained malformed address";
    return scoped_ptr<MaskedInstrument>();
  }

  int expiration_month;
  if (!dictionary.GetInteger("expiration_month", &expiration_month))
    DVLOG(1) << "Response from Google Wallet missing expiration month";

  int expiration_year;
  if (!dictionary.GetInteger("expiration_year", &expiration_year))
    DVLOG(1) << "Response from Google Wallet missing expiration year";

  base::string16 descriptive_name;
  if (!dictionary.GetString("descriptive_name", &descriptive_name))
    DVLOG(1) << "Response from Google Wallet missing descriptive name";

  return scoped_ptr<MaskedInstrument>(new MaskedInstrument(descriptive_name,
                                                           type,
                                                           last_four_digits,
                                                           expiration_month,
                                                           expiration_year,
                                                           address.Pass(),
                                                           status,
                                                           object_id));
}

bool WalletItems::MaskedInstrument::operator==(
    const WalletItems::MaskedInstrument& other) const {
  if (descriptive_name_ != other.descriptive_name_)
    return false;
  if (type_ != other.type_)
    return false;
  if (last_four_digits_ != other.last_four_digits_)
    return false;
  if (expiration_month_ != other.expiration_month_)
    return false;
  if (expiration_year_ != other.expiration_year_)
    return false;
  if (address_) {
    if (other.address_) {
      if (*address_ != *other.address_)
        return false;
    } else {
      return false;
    }
  } else if (other.address_) {
    return false;
  }
  if (status_ != other.status_)
    return false;
  if (object_id_ != other.object_id_)
    return false;
  return true;
}

bool WalletItems::MaskedInstrument::operator!=(
    const WalletItems::MaskedInstrument& other) const {
  return !(*this == other);
}

const WalletItems::MaskedInstrument* WalletItems::GetInstrumentById(
    const std::string& object_id) const {
  if (object_id.empty())
    return NULL;

  for (size_t i = 0; i < instruments_.size(); ++i) {
    if (instruments_[i]->object_id() == object_id)
      return instruments_[i];
  }

  return NULL;
}

bool WalletItems::HasRequiredAction(RequiredAction action) const {
  DCHECK(ActionAppliesToWalletItems(action));
  return std::find(required_actions_.begin(),
                   required_actions_.end(),
                   action) != required_actions_.end();
}

bool WalletItems::SupportsCard(const base::string16& card_number,
                               base::string16* message) const {
  const char* const card_type = CreditCard::GetCreditCardType(card_number);

  if (card_type == kVisaCard ||
      card_type == kMasterCard ||
      card_type == kDiscoverCard) {
    return true;
  }

  if (card_type == kAmericanExpressCard) {
    if (amex_permission_ == AMEX_ALLOWED)
      return true;

    *message = l10n_util::GetStringUTF16(
        IDS_AUTOFILL_CREDIT_CARD_NOT_SUPPORTED_BY_WALLET_FOR_MERCHANT);
    return false;
  }

  *message = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_CREDIT_CARD_NOT_SUPPORTED_BY_WALLET);
   return false;
}

std::string WalletItems::ObfuscatedGaiaId() const {
  if (active_account_index_ >= gaia_accounts_.size())
    return std::string();

  return gaia_accounts_[active_account_index_]->obfuscated_id();
}

base::string16 WalletItems::MaskedInstrument::DisplayName() const {
#if defined(OS_ANDROID)
  // TODO(aruslan): improve this stub implementation.
  return descriptive_name();
#else
  return descriptive_name();
#endif
}

base::string16 WalletItems::MaskedInstrument::DisplayNameDetail() const {
#if defined(OS_ANDROID)
  // TODO(aruslan): improve this stub implementation.
  return address().DisplayName();
#else
  return base::string16();
#endif
}

base::string16 WalletItems::MaskedInstrument::TypeAndLastFourDigits() const {
  // TODO(dbeam): i18n.
  return DisplayStringFromType(type_) + base::ASCIIToUTF16(" - ") +
         last_four_digits();
}

const gfx::Image& WalletItems::MaskedInstrument::CardIcon() const {
  int idr = 0;
  switch (type_) {
    case AMEX:
      idr = IDR_AUTOFILL_CC_AMEX;
      break;

    case DISCOVER:
      idr = IDR_AUTOFILL_CC_DISCOVER;
      break;

    case MASTER_CARD:
      idr = IDR_AUTOFILL_CC_MASTERCARD;
      break;

    case VISA:
      idr = IDR_AUTOFILL_CC_VISA;
      break;

    case SOLO:
    case MAESTRO:
    case SWITCH:
    case UNKNOWN:
      idr = IDR_AUTOFILL_CC_GENERIC;
      break;
  }

  return ResourceBundle::GetSharedInstance().GetImageNamed(idr);
}

base::string16 WalletItems::MaskedInstrument::GetInfo(
    const AutofillType& type,
    const std::string& app_locale) const {
  if (type.group() != CREDIT_CARD)
    return address().GetInfo(type, app_locale);

  switch (type.GetStorableType()) {
    case CREDIT_CARD_NAME:
      return address().recipient_name();

    case CREDIT_CARD_NUMBER:
      return DisplayName();

    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      return base::IntToString16(expiration_year());

    case CREDIT_CARD_VERIFICATION_CODE:
      break;

    case CREDIT_CARD_TYPE:
      return DisplayStringFromType(type_);

    default:
      NOTREACHED();
  }

  return base::string16();
}

WalletItems::LegalDocument::~LegalDocument() {}

scoped_ptr<WalletItems::LegalDocument>
    WalletItems::LegalDocument::CreateLegalDocument(
    const base::DictionaryValue& dictionary) {
  std::string id;
  if (!dictionary.GetString("legal_document_id", &id)) {
    DLOG(ERROR) << "Response from Google Wallet missing legal document id";
    return scoped_ptr<LegalDocument>();
  }

  base::string16 display_name;
  if (!dictionary.GetString("display_name", &display_name)) {
    DLOG(ERROR) << "Response from Google Wallet missing display name";
    return scoped_ptr<LegalDocument>();
  }

  return scoped_ptr<LegalDocument>(new LegalDocument(id, display_name));
}

scoped_ptr<WalletItems::LegalDocument>
    WalletItems::LegalDocument::CreatePrivacyPolicyDocument() {
  return scoped_ptr<LegalDocument>(new LegalDocument(
      GURL(kPrivacyNoticeUrl),
      l10n_util::GetStringUTF16(IDS_AUTOFILL_DIALOG_PRIVACY_POLICY_LINK)));
}

bool WalletItems::LegalDocument::operator==(const LegalDocument& other) const {
  return id_ == other.id_ &&
         url_ == other.url_ &&
         display_name_ == other.display_name_;
}

bool WalletItems::LegalDocument::operator!=(const LegalDocument& other) const {
  return !(*this == other);
}

WalletItems::LegalDocument::LegalDocument(const std::string& id,
                                          const base::string16& display_name)
    : id_(id),
      url_(kLegalDocumentUrl + id),
      display_name_(display_name) {}

WalletItems::LegalDocument::LegalDocument(const GURL& url,
                                          const base::string16& display_name)
    : url_(url),
      display_name_(display_name) {}

WalletItems::WalletItems(const std::vector<RequiredAction>& required_actions,
                         const std::string& google_transaction_id,
                         const std::string& default_instrument_id,
                         const std::string& default_address_id,
                         AmexPermission amex_permission)
    : required_actions_(required_actions),
      google_transaction_id_(google_transaction_id),
      default_instrument_id_(default_instrument_id),
      default_address_id_(default_address_id),
      active_account_index_(std::numeric_limits<size_t>::max()),
      amex_permission_(amex_permission) {}

WalletItems::~WalletItems() {}

scoped_ptr<WalletItems>
    WalletItems::CreateWalletItems(const base::DictionaryValue& dictionary) {
  std::vector<RequiredAction> required_action;
  const base::ListValue* required_action_list;
  if (dictionary.GetList("required_action", &required_action_list)) {
    for (size_t i = 0; i < required_action_list->GetSize(); ++i) {
      std::string action_string;
      if (required_action_list->GetString(i, &action_string)) {
        RequiredAction action = ParseRequiredActionFromString(action_string);
        if (!ActionAppliesToWalletItems(action)) {
          DLOG(ERROR) << "Response from Google wallet with bad required action:"
                         " \"" << action_string << "\"";
          return scoped_ptr<WalletItems>();
        }
        required_action.push_back(action);
      }
    }
  } else {
    DVLOG(1) << "Response from Google wallet missing required actions";
  }

  std::string google_transaction_id;
  if (!dictionary.GetString("google_transaction_id", &google_transaction_id) &&
      required_action.empty()) {
    DLOG(ERROR) << "Response from Google wallet missing google transaction id";
    return scoped_ptr<WalletItems>();
  }

  std::string default_instrument_id;
  if (!dictionary.GetString("default_instrument_id", &default_instrument_id))
    DVLOG(1) << "Response from Google wallet missing default instrument id";

  std::string default_address_id;
  if (!dictionary.GetString("default_address_id", &default_address_id))
    DVLOG(1) << "Response from Google wallet missing default_address_id";

  // obfuscated_gaia_id is deprecated.

  bool amex_disallowed = true;
  if (!dictionary.GetBoolean("amex_disallowed", &amex_disallowed))
    DVLOG(1) << "Response from Google wallet missing the amex_disallowed field";
  AmexPermission amex_permission =
      amex_disallowed ? AMEX_DISALLOWED : AMEX_ALLOWED;

  scoped_ptr<WalletItems> wallet_items(new WalletItems(required_action,
                                                       google_transaction_id,
                                                       default_instrument_id,
                                                       default_address_id,
                                                       amex_permission));
  std::vector<std::string> gaia_accounts;
  const base::ListValue* gaia_profiles;
  if (dictionary.GetList("gaia_profile", &gaia_profiles)) {
    for (size_t i = 0; i < gaia_profiles->GetSize(); ++i) {
      const base::DictionaryValue* account_dict;
      std::string email;
      if (!gaia_profiles->GetDictionary(i, &account_dict))
        continue;

      scoped_ptr<GaiaAccount> gaia_account(
          GaiaAccount::Create(*account_dict));
      if (gaia_account)
        wallet_items->AddAccount(gaia_account.Pass());
    }
  } else {
    DVLOG(1) << "Response from Google wallet missing GAIA accounts";
  }

  const base::ListValue* legal_docs;
  if (dictionary.GetList("required_legal_document", &legal_docs)) {
    for (size_t i = 0; i < legal_docs->GetSize(); ++i) {
      const base::DictionaryValue* legal_doc_dict;
      if (legal_docs->GetDictionary(i, &legal_doc_dict)) {
        scoped_ptr<LegalDocument> legal_doc(
            LegalDocument::CreateLegalDocument(*legal_doc_dict));
        if (legal_doc)
          wallet_items->AddLegalDocument(legal_doc.Pass());
        else
          return scoped_ptr<WalletItems>();
      }
    }

    if (!legal_docs->empty()) {
      // Always append the privacy policy link as well.
      wallet_items->AddLegalDocument(
          LegalDocument::CreatePrivacyPolicyDocument());
    }
  } else {
    DVLOG(1) << "Response from Google wallet missing legal docs";
  }

  const base::ListValue* instruments;
  if (dictionary.GetList("instrument", &instruments)) {
    for (size_t i = 0; i < instruments->GetSize(); ++i) {
      const base::DictionaryValue* instrument_dict;
      if (instruments->GetDictionary(i, &instrument_dict)) {
        scoped_ptr<MaskedInstrument> instrument(
            MaskedInstrument::CreateMaskedInstrument(*instrument_dict));
        if (instrument)
          wallet_items->AddInstrument(instrument.Pass());
      }
    }
  } else {
    DVLOG(1) << "Response from Google wallet missing instruments";
  }

  const base::ListValue* addresses;
  if (dictionary.GetList("address", &addresses)) {
    for (size_t i = 0; i < addresses->GetSize(); ++i) {
      const base::DictionaryValue* address_dict;
      if (addresses->GetDictionary(i, &address_dict)) {
        scoped_ptr<Address> address(
            Address::CreateAddressWithID(*address_dict));
        if (address)
          wallet_items->AddAddress(address.Pass());
      }
    }
  } else {
    DVLOG(1) << "Response from Google wallet missing addresses";
  }

  const base::ListValue* allowed_shipping_countries;
  if (dictionary.GetList("allowed_shipping_spec_by_country",
                         &allowed_shipping_countries)) {
    for (size_t i = 0; i < allowed_shipping_countries->GetSize(); ++i) {
      const base::DictionaryValue* country_spec;
      std::string country_code;
      if (allowed_shipping_countries->GetDictionary(i, &country_spec) &&
          country_spec->GetString("country_code", &country_code)) {
        wallet_items->AddAllowedShippingCountry(country_code);
      }
    }
  } else {
    DVLOG(1) << "Response from Google wallet missing allowed shipping"
                " countries";
  }

  return wallet_items.Pass();
}

void WalletItems::AddAccount(scoped_ptr<GaiaAccount> account) {
  if (account->index() != gaia_accounts_.size()) {
    DVLOG(1) << "Tried to add account out of order";
    return;
  }

  if (account->is_active())
    active_account_index_ = account->index();

  gaia_accounts_.push_back(account.release());
}

bool WalletItems::operator==(const WalletItems& other) const {
  return google_transaction_id_ == other.google_transaction_id_ &&
         default_instrument_id_ == other.default_instrument_id_ &&
         default_address_id_ == other.default_address_id_ &&
         required_actions_ == other.required_actions_ &&
         // This check is technically redundant, but is useful for tests.
         ObfuscatedGaiaId() == other.ObfuscatedGaiaId() &&
         active_account_index() == other.active_account_index() &&
         VectorsAreEqual<GaiaAccount>(gaia_accounts(),
                                      other.gaia_accounts()) &&
         VectorsAreEqual<MaskedInstrument>(instruments(),
                                            other.instruments()) &&
         VectorsAreEqual<Address>(addresses(), other.addresses()) &&
         VectorsAreEqual<LegalDocument>(legal_documents(),
                                         other.legal_documents()) &&
         allowed_shipping_countries() == other.allowed_shipping_countries();
}

bool WalletItems::operator!=(const WalletItems& other) const {
  return !(*this == other);
}

}  // namespace wallet
}  // namespace autofill
