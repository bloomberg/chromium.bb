// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_GAIA_ACCOUNTS_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_GAIA_ACCOUNTS_H_

#include <string>

#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
}

namespace autofill {
namespace wallet {

class GaiaAccount {
 public:
  ~GaiaAccount();

  // Returns an empty scoped_ptr if input is invalid, otherwise a valid GAIA
  // account.
  static scoped_ptr<GaiaAccount> Create(
      const base::DictionaryValue& dictionary);

  static scoped_ptr<GaiaAccount> CreateForTesting(
      const std::string& email_address,
      const std::string& obfuscated_id,
      size_t index,
      bool is_active);

  bool operator==(const GaiaAccount& other) const;
  bool operator!=(const GaiaAccount& other) const;

  const std::string& email_address() const { return email_address_; }
  const std::string& obfuscated_id() const { return obfuscated_id_; }
  size_t index() const { return index_; }
  bool is_active() const { return is_active_; }

 private:
  GaiaAccount(const std::string& email_address,
              const std::string& obfuscated_id,
              size_t index,
              bool is_active);

  std::string email_address_;
  std::string obfuscated_id_;
  size_t index_;
  bool is_active_;

  DISALLOW_COPY_AND_ASSIGN(GaiaAccount);
};

}  // namespace wallet
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_GAIA_ACCOUNTS_H_
