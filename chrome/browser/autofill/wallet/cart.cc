// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill/wallet/cart.h"

#include "base/values.h"

namespace autofill {
namespace wallet {

Cart::Cart(const std::string& total_price, const std::string& currency_code)
    : total_price_(total_price), currency_code_(currency_code) {}

Cart::~Cart() {}

scoped_ptr<base::DictionaryValue> Cart::ToDictionary() const {
  base::DictionaryValue* dict = new base::DictionaryValue();
  dict->SetString("total_price", total_price_);
  dict->SetString("currency_code", currency_code_);
  return scoped_ptr<base::DictionaryValue>(dict);
}

}  // namespace wallet
}  // namespace autofill
