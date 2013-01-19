// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/wallet/full_wallet.h"

#include "base/logging.h"
#include "base/string_number_conversions.h"
#include "base/values.h"

namespace {

const size_t kPanSize = 16;
const size_t kBinSize = 6;
const size_t kCvnSize = 3;

}  // anonymous namespace

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

scoped_ptr<FullWallet>
    FullWallet::CreateFullWallet(const DictionaryValue& dictionary) {
  const ListValue* required_actions_list;
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

  const DictionaryValue* billing_address_dict;
  if (!dictionary.GetDictionary("billing_address", &billing_address_dict)) {
    DLOG(ERROR) << "Response from Google wallet missing billing address";
    return scoped_ptr<FullWallet>();
  }

  scoped_ptr<Address> billing_address =
      Address::CreateAddressWithID(*billing_address_dict);
  if (!billing_address.get()) {
    DLOG(ERROR) << "Response from Google wallet has malformed billing address";
    return scoped_ptr<FullWallet>();
  }

  const DictionaryValue* shipping_address_dict;
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

const std::string& FullWallet::GetPan(void* otp, size_t length) {
  if (cvn_.empty())
    DecryptCardInfo(reinterpret_cast<uint8*>(otp), length);
  return pan_;
}

const std::string& FullWallet::GetCvn(void* otp, size_t length) {
  if (pan_.empty())
    DecryptCardInfo(reinterpret_cast<uint8*>(otp), length);
  return cvn_;
}

void FullWallet::DecryptCardInfo(uint8* otp, size_t length) {
  std::vector<uint8> operating_data;
  // Convert |encrypted_rest_| to bytes so we can decrypt it with |otp|.
  if (!base::HexStringToBytes(encrypted_rest_, &operating_data)) {
    DLOG(ERROR) << "Failed to parse encrypted rest";
    return;
  }

  // Ensure |otp| and |encrypted_rest_| are of the same length otherwise
  // something has gone wrong and we can't decrypt the data.
  DCHECK_EQ(length, operating_data.size());

  scoped_array<uint8> result(new uint8[length]);
  // XOR |otp| with the encrypted data to decrypt.
  for (size_t i = 0; i < length; ++i)
    result.get()[i] = otp[i] ^ operating_data[i];

  // There is no uint8* to int64 so convert the decrypted data to hex and then
  // parse the hex to an int64 before getting the int64 as a string.
  std::string hex_decrypted = base::HexEncode(result.get(), length);

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

}  // namespace wallet

