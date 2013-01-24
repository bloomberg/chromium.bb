// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_AUTOFILL_WALLET_REQUIRED_ACTION_H_
#define CHROME_BROWSER_AUTOFILL_WALLET_REQUIRED_ACTION_H_

#include <string>

namespace wallet {

// Required actions are steps that must be taken before the current transaction
// can proceed. Examples of this is include accepting the Terms of Service to
// use Google Wallet (happens on first use or when the ToS are updated) or
// typing a CVC when it's necessary verify the current user has access to the
// backing card.
enum RequiredAction {
  UNKNOWN_TYPE = 0,  // Catch all type.
  SETUP_WALLET,
  ACCEPT_TOS,
  GAIA_AUTH,
  UPDATE_EXPIRATION_DATE,
  UPGRADE_MIN_ADDRESS,
  INVALID_FORM_FIELD,
  CVC_RISK_CHALLENGE,
  PASSIVE_GAIA_AUTH,
};

// Static helper functions to determine if an RequiredAction applies to either a
// FullWallet or WalletItems.
bool ActionAppliesToFullWallet(RequiredAction action);
bool ActionAppliesToWalletItems(RequiredAction action);

// Turn a string value of the parsed JSON response into an RequiredAction.
RequiredAction ParseRequiredActionFromString(const std::string& str);

}  // namespace wallet

#endif  // CHROME_BROWSER_AUTOFILL_WALLET_REQUIRED_ACTION_H_
