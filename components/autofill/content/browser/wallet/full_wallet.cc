// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/wallet/full_wallet.h"

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "base/values.h"
#include "components/autofill/core/browser/autofill_type.h"
#include "components/autofill/core/browser/credit_card.h"

namespace {

const size_t kPanSize = 16;
const size_t kBinSize = 6;
const size_t kCvnSize = 3;
const size_t kEncryptedRestSize = 12;

}  // anonymous namespace

namespace autofill {
namespace wallet {

FullWallet::FullWallet(int expiration_month,
                       int expiration_year,
                       const std::string& iin,
                       const std::string& encrypted_rest,
                       scoped_ptr<Address> billing_address,
                       scoped_ptr<Address> shipping_address,
                       const std::vector<RequiredAction>& required_actions)
    : expiration_month_(expiration_month),
      expiration_year_(expiration_year),
      iin_(iin),
      encrypted_rest_(encrypted_rest),
      billing_address_(billing_address.Pass()),
      shipping_address_(shipping_address.Pass()),
      required_actions_(required_actions) {
  DCHECK(required_actions_.size() > 0 || billing_address_.get());
}

FullWallet::~FullWallet() {}

// static
scoped_ptr<FullWallet>
    FullWallet::CreateFullWallet(const base::DictionaryValue& dictionary) {
  const base::ListValue* required_actions_list;
  std::vector<RequiredAction> required_actions;
  if (dictionary.GetList("required_action", &required_actions_list)) {
    for (size_t i = 0; i < required_actions_list->GetSize(); ++i) {
      std::string action_string;
      if (required_actions_list->GetString(i, &action_string)) {
        RequiredAction action = ParseRequiredActionFromString(action_string);
        if (!ActionAppliesToFullWallet(action)) {
          DLOG(ERROR) << "Response from Google wallet with bad required action:"
                         " \"" << action_string << "\"";
          return scoped_ptr<FullWallet>();
        }
        required_actions.push_back(action);
      }
    }
    if (required_actions.size() > 0) {
      return scoped_ptr<FullWallet>(new FullWallet(-1,
                                                   -1,
                                                   std::string(),
                                                   std::string(),
                                                   scoped_ptr<Address>(),
                                                   scoped_ptr<Address>(),
                                                   required_actions));
    }
  } else {
    DVLOG(1) << "Response from Google wallet missing required actions";
  }

  int expiration_month;
  if (!dictionary.GetInteger("expiration_month", &expiration_month)) {
    DLOG(ERROR) << "Response from Google wallet missing expiration month";
    return scoped_ptr<FullWallet>();
  }

  int expiration_year;
  if (!dictionary.GetInteger("expiration_year", &expiration_year)) {
    DLOG(ERROR) << "Response from Google wallet missing expiration year";
    return scoped_ptr<FullWallet>();
  }

  std::string iin;
  if (!dictionary.GetString("iin", &iin)) {
    DLOG(ERROR) << "Response from Google wallet missing iin";
    return scoped_ptr<FullWallet>();
  }

  std::string encrypted_rest;
  if (!dictionary.GetString("rest", &encrypted_rest)) {
    DLOG(ERROR) << "Response from Google wallet missing rest";
    return scoped_ptr<FullWallet>();
  }

  const base::DictionaryValue* billing_address_dict;
  if (!dictionary.GetDictionary("billing_address", &billing_address_dict)) {
    DLOG(ERROR) << "Response from Google wallet missing billing address";
    return scoped_ptr<FullWallet>();
  }

  scoped_ptr<Address> billing_address =
      Address::CreateAddress(*billing_address_dict);
  if (!billing_address.get()) {
    DLOG(ERROR) << "Response from Google wallet has malformed billing address";
    return scoped_ptr<FullWallet>();
  }

  const base::DictionaryValue* shipping_address_dict;
  scoped_ptr<Address> shipping_address;
  if (dictionary.GetDictionary("shipping_address", &shipping_address_dict)) {
    shipping_address =
        Address::CreateAddressWithID(*shipping_address_dict);
  } else {
    DVLOG(1) << "Response from Google wallet missing shipping address";
  }

  return scoped_ptr<FullWallet>(new FullWallet(expiration_month,
                                               expiration_year,
                                               iin,
                                               encrypted_rest,
                                               billing_address.Pass(),
                                               shipping_address.Pass(),
                                               required_actions));
}

// static
scoped_ptr<FullWallet>
    FullWallet::CreateFullWalletFromClearText(
        int expiration_month,
        int expiration_year,
        const std::string& pan,
        const std::string& cvn,
        scoped_ptr<Address> billing_address,
        scoped_ptr<Address> shipping_address) {
  DCHECK(billing_address);
  DCHECK(!pan.empty());
  DCHECK(!cvn.empty());

  scoped_ptr<FullWallet> wallet(new FullWallet(
      expiration_month,
      expiration_year,
      std::string(),  // no iin -- clear text pan/cvn are set below.
      std::string(),  // no encrypted_rest -- clear text pan/cvn are set below.
      billing_address.Pass(),
      shipping_address.Pass(),
      std::vector<RequiredAction>()));  // no required actions in clear text.
  wallet->pan_ = pan;
  wallet->cvn_ = cvn;
  return wallet.Pass();
}

base::string16 FullWallet::GetInfo(const std::string& app_locale,
                                   const AutofillType& type) {
  switch (type.GetStorableType()) {
    case CREDIT_CARD_NUMBER:
      return base::ASCIIToUTF16(GetPan());

    case CREDIT_CARD_NAME:
      return billing_address()->recipient_name();

    case CREDIT_CARD_VERIFICATION_CODE:
      return base::ASCIIToUTF16(GetCvn());

    case CREDIT_CARD_EXP_MONTH:
      if (expiration_month() == 0)
        return base::string16();
      return base::IntToString16(expiration_month());

    case CREDIT_CARD_EXP_4_DIGIT_YEAR:
      if (expiration_year() == 0)
        return base::string16();
      return base::IntToString16(expiration_year());

    case CREDIT_CARD_EXP_2_DIGIT_YEAR:
      if (expiration_year() == 0)
        return base::string16();
      return base::IntToString16(expiration_year() % 100);

    case CREDIT_CARD_EXP_DATE_2_DIGIT_YEAR:
      if (expiration_month() == 0 || expiration_year() == 0)
        return base::string16();
      return base::IntToString16(expiration_month()) + base::ASCIIToUTF16("/") +
             base::IntToString16(expiration_year() % 100);

    case CREDIT_CARD_EXP_DATE_4_DIGIT_YEAR:
      if (expiration_month() == 0 || expiration_year() == 0)
            return base::string16();
      return base::IntToString16(expiration_month()) + base::ASCIIToUTF16("/") +
             base::IntToString16(expiration_year());

    case CREDIT_CARD_TYPE: {
      const char* const internal_type =
          CreditCard::GetCreditCardType(base::ASCIIToUTF16(GetPan()));
      if (internal_type == kGenericCard)
        return base::string16();
      return CreditCard::TypeForDisplay(internal_type);
    }

    default: {
      switch (type.group()) {
        case NAME_BILLING:
        case PHONE_BILLING:
        case ADDRESS_BILLING:
          return billing_address_->GetInfo(type, app_locale);

        case CREDIT_CARD:
          NOTREACHED();

        default:
          return shipping_address_->GetInfo(type, app_locale);
      }
    }
  }
}

bool FullWallet::HasRequiredAction(RequiredAction action) const {
  DCHECK(ActionAppliesToFullWallet(action));
  return std::find(required_actions_.begin(),
                   required_actions_.end(),
                   action) != required_actions_.end();
}

base::string16 FullWallet::TypeAndLastFourDigits() {
  CreditCard card;
  card.SetRawInfo(CREDIT_CARD_NUMBER, base::ASCIIToUTF16(GetPan()));
  return card.TypeAndLastFourDigits();
}

const std::string& FullWallet::GetPan() {
  if (pan_.empty())
    DecryptCardInfo();
  return pan_;
}

bool FullWallet::operator==(const FullWallet& other) const {
  if (expiration_month_ != other.expiration_month_)
    return false;

  if (expiration_year_ != other.expiration_year_)
    return false;

  if (iin_ != other.iin_)
    return false;

  if (encrypted_rest_ != other.encrypted_rest_)
    return false;

  if (billing_address_.get() && other.billing_address_.get()) {
    if (*billing_address_.get() != *other.billing_address_.get())
      return false;
  } else if (billing_address_.get() || other.billing_address_.get()) {
    return false;
  }

  if (shipping_address_.get() && other.shipping_address_.get()) {
    if (*shipping_address_.get() != *other.shipping_address_.get())
      return false;
  } else if (shipping_address_.get() || other.shipping_address_.get()) {
    return false;
  }

  if (required_actions_ != other.required_actions_)
    return false;

  return true;
}

bool FullWallet::operator!=(const FullWallet& other) const {
  return !(*this == other);
}

void FullWallet::DecryptCardInfo() {
  // |encrypted_rest_| must be of length |kEncryptedRestSize| in order for
  // decryption to succeed and the server will not pad it with zeros.
  while (encrypted_rest_.size() < kEncryptedRestSize) {
    encrypted_rest_ = '0' + encrypted_rest_;
  }

  DCHECK_EQ(kEncryptedRestSize, encrypted_rest_.size());

  std::vector<uint8> operating_data;
  // Convert |encrypted_rest_| to bytes so we can decrypt it with |otp|.
  if (!base::HexStringToBytes(encrypted_rest_, &operating_data)) {
    DLOG(ERROR) << "Failed to parse encrypted rest";
    return;
  }

  // Ensure |one_time_pad_| and |encrypted_rest_| are of the same length
  // otherwise something has gone wrong and we can't decrypt the data.
  DCHECK_EQ(one_time_pad_.size(), operating_data.size());

  std::vector<uint8> results;
  // XOR |otp| with the encrypted data to decrypt.
  for (size_t i = 0; i < one_time_pad_.size(); ++i)
    results.push_back(one_time_pad_[i] ^ operating_data[i]);

  // There is no uint8* to int64 so convert the decrypted data to hex and then
  // parse the hex to an int64 before getting the int64 as a string.
  std::string hex_decrypted = base::HexEncode(&(results[0]), results.size());

  int64 decrypted;
  if (!base::HexStringToInt64(hex_decrypted, &decrypted)) {
    DLOG(ERROR) << "Failed to parse decrypted data in hex to int64";
    return;
  }
  std::string card_info = base::Int64ToString(decrypted);

  size_t padded_length = kPanSize - kBinSize + kCvnSize;
  // |card_info| is PAN without the IIN concatenated with the CVN, i.e.
  // PANPANPANPCVN. If what was decrypted is not of that size the front needs
  // to be padded with 0's until it is.
  if (card_info.size() != padded_length)
    card_info.insert(card_info.begin(), padded_length - card_info.size(), '0');

  // Separate out the PAN from the CVN.
  size_t split = kPanSize - kBinSize;
  cvn_ = card_info.substr(split);
  pan_ = iin_ + card_info.substr(0, split);
}

const std::string& FullWallet::GetCvn() {
  if (cvn_.empty())
    DecryptCardInfo();
  return cvn_;
}

}  // namespace wallet
}  // namespace autofill
