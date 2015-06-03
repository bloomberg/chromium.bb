// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_switches.h"

namespace autofill {
namespace switches {

// Disables using device's camera to scan a new credit card when filling out a
// credit card form.
const char kDisableCreditCardScan[]         = "disable-credit-card-scan";

// Disables the experiment for the password manager to only fill on account
// selection, rather than autofilling on page load.
const char kDisableFillOnAccountSelect[]    = "disable-fill-on-account-select";

// Force hiding the local save checkbox in the autofill dialog box for getting
// the full credit card number for a wallet card. The card will never be stored
// locally.
const char kDisableOfferStoreUnmaskedWalletCards[] =
    "disable-offer-store-unmasked-wallet-cards";

// Disables password generation when we detect that the user is going through
// account creation.
const char kDisablePasswordGeneration[]     = "disable-password-generation";

// The "disable" flag for kEnableSingleClickAutofill.
const char kDisableSingleClickAutofill[]    = "disable-single-click-autofill";

const char kEnableAccessorySuggestionView[] =
    "enable-autofill-keyboard-accessory-view";

// Enables using device's camera to scan a new credit card when filling out a
// credit card form.
const char kEnableCreditCardScan[]          = "enable-credit-card-scan";

// Enables the experiment for the password manager to only fill on account
// selection, rather than autofilling on page load, with highlighting of fields.
const char kEnableFillOnAccountSelect[]     = "enable-fill-on-account-select";

// Enables the experiment for the password manager to only fill on account
// selection, rather than autofilling on page load, with no highlighting of
// fields.
const char kEnableFillOnAccountSelectNoHighlighting[] =
    "enable-fill-on-account-select-no-highlighting";

// Force showing the local save checkbox in the autofill dialog box for getting
// the full credit card number for a wallet card.
const char kEnableOfferStoreUnmaskedWalletCards[] =
    "enable-offer-store-unmasked-wallet-cards";

// Enables password generation when we detect that the user is going through
// account creation.
const char kEnablePasswordGeneration[]      = "enable-password-generation";

// Enables/disables suggestions without typing anything (on first click).
const char kEnableSingleClickAutofill[]     = "enable-single-click-autofill";

// Enables syncing usage counts and last use dates of Wallet addresses and
// cards.
const char kEnableWalletMetadataSync[]      = "enable-wallet-metadata-sync";

// Ignores autocomplete="off" for Autofill data (profiles + credit cards).
const char kIgnoreAutocompleteOffForAutofill[] =
    "ignore-autocomplete-off-autofill";

// Removes the requirement that we recieved a ping from the autofill servers
// and that the user doesn't have the given form blacklisted. Used in testing.
const char kLocalHeuristicsOnlyForPasswordGeneration[] =
    "local-heuristics-only-for-password-generation";

// Annotates forms with Autofill field type predictions.
const char kShowAutofillTypePredictions[]   = "show-autofill-type-predictions";

// Secure service URL for Online Wallet service. Used as the base url to escrow
// credit card numbers.
const char kWalletSecureServiceUrl[]        = "wallet-secure-service-url";

// Service URL for Online Wallet service. Used as the base url for Online Wallet
// API calls.
const char kWalletServiceUrl[]              = "wallet-service-url";

// Use the sandbox Online Wallet service URL (for developer testing).
const char kWalletServiceUseSandbox[]       = "wallet-service-use-sandbox";

}  // namespace switches
}  // namespace autofill
