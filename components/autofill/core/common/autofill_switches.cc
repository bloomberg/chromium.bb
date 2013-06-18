// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_switches.h"

namespace autofill {
namespace switches {

// Flag used to tell Chrome the Autochecout whitelist url.
const char kAutocheckoutWhitelistUrl[]      = "autocheckout-whitelist-url";

// Flag used to tell Chrome the base url of the Autofill service.
const char kAutofillServiceUrl[]            = "autofill-service-url";

// Bypass autocheckout whitelist check, so all sites are enabled.
const char kBypassAutocheckoutWhitelist[]   = "bypass-autocheckout-whitelist";

// Disables an interactive autocomplete UI. See kEnableInteractiveAutocomplete
// for a description.
const char kDisableInteractiveAutocomplete[] =
    "disable-interactive-autocomplete";

// Enable autofill for new elements like checkboxes. crbug.com/157636
const char kEnableExperimentalFormFilling[] =
    "enable-experimental-form-filling";

// Enables an interactive autocomplete UI and a way to invoke this UI from
// WebKit by enabling HTMLFormElement#requestAutocomplete (and associated
// autocomplete* events and logic).
const char kEnableInteractiveAutocomplete[] = "enable-interactive-autocomplete";

// Annotates forms with Autofill field type predictions.
const char kShowAutofillTypePredictions[]   = "show-autofill-type-predictions";

// Secure service URL for Online Wallet service. Used as the base url to escrow
// credit card numbers.
const char kWalletSecureServiceUrl[]        = "wallet-secure-service-url";

// Service URL for Online Wallet service. Used as the base url for Online Wallet
// API calls.
const char kWalletServiceUrl[]              = "wallet-service-url";

// Enable production Online Wallet service. If this flag is not set, the sandbox
// service will be used.
const char kWalletServiceUseProd[]          = "wallet-service-use-prod";

}  // namespace switches
}  // namespace autofill
