// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_FULL_WALLET_H_
#define COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_FULL_WALLET_H_

#include <string>
#include <vector>

#include "base/basictypes.h"
#include "base/gtest_prod_util.h"
#include "base/memory/scoped_ptr.h"
#include "base/strings/string16.h"
#include "components/autofill/content/browser/wallet/wallet_address.h"

namespace base {
class DictionaryValue;
}

namespace autofill {

class AutofillType;

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

  // Returns a wallet built from the provided clear-text data.
  // Data is not validated; |pan|, |cvn| and |billing_address| must be set.
  static scoped_ptr<FullWallet>
      CreateFullWalletFromClearText(int expiration_month,
                                    int expiration_year,
                                    const std::string& pan,
                                    const std::string& cvn,
                                    scoped_ptr<Address> billing_address,
                                    scoped_ptr<Address> shipping_address);

  // Returns corresponding data for |type|.
  base::string16 GetInfo(const std::string& app_locale,
                         const AutofillType& type);

  // The type of the card that this FullWallet contains and the last four digits
  // like this "Visa - 4111".
  base::string16 TypeAndLastFourDigits();

  // Decrypts and returns the primary account number (PAN) using the generated
  // one time pad, |one_time_pad_|.
  const std::string& GetPan();

  bool operator==(const FullWallet& other) const;
  bool operator!=(const FullWallet& other) const;

  const Address* billing_address() const { return billing_address_.get(); }
  const Address* shipping_address() const { return shipping_address_.get(); }

  int expiration_month() const { return expiration_month_; }
  int expiration_year() const { return expiration_year_; }

  void set_one_time_pad(const std::vector<uint8>& one_time_pad) {
    one_time_pad_ = one_time_pad;
  }

 private:
  friend class FullWalletTest;
  friend scoped_ptr<FullWallet> GetTestFullWallet();
  friend scoped_ptr<FullWallet> GetTestFullWalletInstrumentOnly();
  FRIEND_TEST_ALL_PREFIXES(FullWalletTest, CreateFullWallet);
  FRIEND_TEST_ALL_PREFIXES(FullWalletTest, RestLengthCorrectDecryptionTest);
  FRIEND_TEST_ALL_PREFIXES(FullWalletTest, RestLengthUnderDecryptionTest);
  FRIEND_TEST_ALL_PREFIXES(FullWalletTest, GetCreditCardInfo);

  FullWallet(int expiration_month,
             int expiration_year,
             const std::string& iin,
             const std::string& encrypted_rest,
             scoped_ptr<Address> billing_address,
             scoped_ptr<Address> shipping_address);

  // Decrypts both |pan_| and |cvn_|.
  void DecryptCardInfo();

  // Decrypts and returns the card verification number (CVN) using the generated
  // one time pad, |one_time_pad_|.
  const std::string& GetCvn();

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

  // The one time pad used for FullWallet encryption.
  std::vector<uint8> one_time_pad_;

  DISALLOW_COPY_AND_ASSIGN(FullWallet);
};

}  // namespace wallet
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_BROWSER_WALLET_FULL_WALLET_H_
