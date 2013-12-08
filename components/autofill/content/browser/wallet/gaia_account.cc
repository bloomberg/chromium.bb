// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/content/browser/wallet/gaia_account.h"

#include "base/logging.h"
#include "base/values.h"

namespace autofill {

namespace wallet {

GaiaAccount::~GaiaAccount() {}

// static
scoped_ptr<GaiaAccount> GaiaAccount::Create(
    const base::DictionaryValue& dictionary) {
  std::string email_address;
  if (!dictionary.GetString("buyer_email", &email_address)) {
    DLOG(ERROR) << "GAIA account: missing email address";
    return scoped_ptr<GaiaAccount>();
  }

  std::string obfuscated_id;
  if (!dictionary.GetString("gaia_id", &obfuscated_id)) {
    DLOG(ERROR) << "GAIA account: missing GAIA id";
    return scoped_ptr<GaiaAccount>();
  }

  int index = 0;
  if (!dictionary.GetInteger("gaia_index", &index) ||
      index < 0) {
    DLOG(ERROR) << "GAIA account: missing or bad GAIA index";
    return scoped_ptr<GaiaAccount>();
  }

  bool is_active = false;
  if (!dictionary.GetBoolean("is_active", &is_active)) {
    DLOG(ERROR) << "GAIA account: missing is_active";
    return scoped_ptr<GaiaAccount>();
  }

  return scoped_ptr<GaiaAccount>(new GaiaAccount(email_address,
                                                 obfuscated_id,
                                                 index,
                                                 is_active));
}

// static
scoped_ptr<GaiaAccount> GaiaAccount::CreateForTesting(
    const std::string& email_address,
    const std::string& obfuscated_id,
    size_t index,
    bool is_active) {
  scoped_ptr<GaiaAccount> account(new GaiaAccount(email_address,
                                                  obfuscated_id,
                                                  index,
                                                  is_active));
  return account.Pass();
}

bool GaiaAccount::operator==(const GaiaAccount& other) const {
  return email_address_ == other.email_address_ &&
      obfuscated_id_ == other.obfuscated_id_ &&
      index_ == other.index_ &&
      is_active_ == other.is_active_;
}

bool GaiaAccount::operator!=(const GaiaAccount& other) const {
  return !(*this == other);
}

GaiaAccount::GaiaAccount(const std::string& email_address,
                         const std::string& obfuscated_id,
                         size_t index,
                         bool is_active)
    : email_address_(email_address),
      obfuscated_id_(obfuscated_id),
      index_(index),
      is_active_(is_active) {}

}  // namespace wallet

}  // namespace autofill
