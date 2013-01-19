// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_WALLET_WALLET_TEST_UTIL_H_
#define CHROME_BROWSER_AUTOFILL_WALLET_WALLET_TEST_UTIL_H_

#include "base/memory/scoped_ptr.h"

namespace wallet {

class Instrument;
class Address;

scoped_ptr<Instrument> GetTestInstrument();
scoped_ptr<Address> GetTestShippingAddress();

}  // namespace wallet

#endif  // CHROME_BROWSER_AUTOFILL_WALLET_WALLET_TEST_UTIL_H_
