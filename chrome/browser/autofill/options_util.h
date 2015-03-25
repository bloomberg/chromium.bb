// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_OPTIONS_UTIL_H_
#define CHROME_BROWSER_AUTOFILL_OPTIONS_UTIL_H_

class Profile;

namespace autofill {

// Decides whether the Autofill Wallet integration is available (i.e. can be
// enabled or disabled by the user).
bool WalletIntegrationAvailableForProfile(Profile* profile);

}  // namespace autofill

#endif  // CHROME_BROWSER_AUTOFILL_OPTIONS_UTIL_H_
