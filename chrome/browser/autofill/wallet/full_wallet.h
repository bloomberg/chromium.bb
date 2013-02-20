// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_WALLET_FULL_WALLET_H_
#define CHROME_BROWSER_AUTOFILL_WALLET_FULL_WALLET_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/autofill/wallet/required_action.h"
#include "chrome/browser/autofill/wallet/wallet_address.h"

namespace base {
class DictionaryValue;
}

namespace autofill {
namespace wallet {

class FullWalletTest;

// FullWallet contains all the information a merchant requires from a user for
// that user to make a purchase. This includes:
//  - billing information
//  - shipping information
//  - a proxy card for the backing card selected from a user's wallet items
class FullWallet {
 public:
  ~FullWallet();

  // Returns an empty scoped_ptr if the input invalid, an empty wallet with
  // required actions if there are any, or a valid wallet.
  static scoped_ptr<FullWallet>
      CreateFullWallet(const base::DictionaryValue& dictionary);

  // Decrypts and returns the primary account number (PAN) using the generated
  // one time pad, |one_time_pad_|.
  const std::string& GetPan();

  // Decrypts and returns the card verification number (CVN) using the generated
  // one time pad, |one_time_pad_|.
  const std::string& GetCvn();

  bool operator==(const FullWallet& other) const;
  bool operator!=(const FullWallet& other) const;

  // If there are required actions |billing_address_| might contain NULL.
  const Address* billing_address() const { return billing_address_.get(); }

  // If there are required actions or shipping address is not required
  // |shipping_address_| might contain NULL.
  const Address* shipping_address() const { return shipping_address_.get(); }

  const std::vector<RequiredAction>& required_actions() const {
    return required_actions_;
  }
  int expiration_month() const { return expiration_month_; }
  int expiration_year() const { return expiration_year_; }

  void set_one_time_pad(const std::vector<uint8>& one_time_pad) {
    one_time_pad_ = one_time_pad;
  }

 private:
  friend class FullWalletTest;
  FRIEND_TEST_ALL_PREFIXES(FullWalletTest, CreateFullWallet);
  FRIEND_TEST_ALL_PREFIXES(FullWalletTest, CreateFullWalletWithRequiredActions);
  FullWallet(int expiration_month,
             int expiration_year,
             const std::string& iin,
             const std::string& encrypted_rest,
             scoped_ptr<Address> billing_address,
             scoped_ptr<Address> shipping_address,
             const std::vector<RequiredAction>& required_actions);
  void DecryptCardInfo();

  // The expiration month of the proxy card. It should be 1-12.
  int expiration_month_;

  // The expiration year of the proxy card. It should be a 4-digit year.
  int expiration_year_;

  // Primary account number (PAN). Its format is \d{16}.
  std::string pan_;

  // Card verification number (CVN). Its format is \d{3}.
  std::string cvn_;

  // Issuer identification number (IIN). Its format is \d{6}.
  std::string iin_;

  // Encrypted concatentation of CVN and PAN without IIN
  std::string encrypted_rest_;

  // The billing address of the backing instrument.
  scoped_ptr<Address> billing_address_;

  // The shipping address for the transaction.
  scoped_ptr<Address> shipping_address_;

  // Actions that must be completed by the user before a FullWallet can be
  // issued to them by the Online Wallet service.
  std::vector<RequiredAction> required_actions_;

  // The one time pad used for FullWallet encryption.
  std::vector<uint8> one_time_pad_;

  DISALLOW_COPY_AND_ASSIGN(FullWallet);
};

}  // namespace wallet
}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_WALLET_FULL_WALLET_H_
