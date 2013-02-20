// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_WALLET_CART_H_
#define CHROME_BROWSER_AUTOFILL_WALLET_CART_H_

#include <string>

#include "base/basictypes.h"
#include "base/memory/scoped_ptr.h"

namespace base {
class DictionaryValue;
}

namespace autofill {
namespace wallet {

// Container object for purchase data provided by the browser. The enclosed data
// is required to request a FullWallet from Online Wallet in order to set
// spending limits on the generated proxy card. If the actual amount is not
// available, the maximum allowable value should be used: $1850 USD. Online
// Wallet is designed to accept price information in addition to information
// about the items being purchased hence the name Cart.
class Cart {
 public:
  Cart(const std::string& total_price, const std::string& currency_code);
  ~Cart();
  const std::string& total_price() const { return total_price_; }
  const std::string& currency_code() const { return currency_code_; }
  scoped_ptr<base::DictionaryValue> ToDictionary() const;

 private:
  // |total_price_| must be a formatted as a double with no more than two
  // decimals, e.g. 100.99
  std::string total_price_;

  // |currency_code_| must be one of the ISO 4217 currency codes, e.g. USD.
  std::string currency_code_;

  DISALLOW_COPY_AND_ASSIGN(Cart);
};

}  // namespace wallet
}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_WALLET_CART_H_
