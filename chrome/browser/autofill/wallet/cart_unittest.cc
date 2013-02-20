// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/values.h"
#include "chrome/browser/autofill/wallet/cart.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace autofill {
namespace wallet {

TEST(Cart, ToDictionary) {
  base::DictionaryValue expected;
  expected.SetString("total_price", "total_price");
  expected.SetString("currency_code", "currency_code");
  Cart cart("total_price", "currency_code");
  ASSERT_TRUE(expected.Equals(cart.ToDictionary().get()));
}

}  // namespace wallet
}  // namespace autofill
