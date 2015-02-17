// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_SWITCHES_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_SWITCHES_H_

namespace autofill {
namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kDisableCreditCardScan[];
extern const char kDisableFillOnAccountSelect[];
extern const char kDisablePasswordGeneration[];
extern const char kDisableSingleClickAutofill[];
extern const char kEnableCreditCardScan[];
extern const char kEnableFillOnAccountSelect[];
extern const char kEnableFillOnAccountSelectNoHighlighting[];
extern const char kEnablePasswordGeneration[];
extern const char kEnablePasswordSaveOnInPageNavigation[];
extern const char kEnableSingleClickAutofill[];
extern const char kIgnoreAutocompleteOffForAutofill[];
extern const char kLocalHeuristicsOnlyForPasswordGeneration[];
extern const char kRespectAutocompleteOffForAutofill[];
extern const char kShowAutofillTypePredictions[];
extern const char kWalletSecureServiceUrl[];
extern const char kWalletServiceUrl[];
extern const char kWalletServiceUseSandbox[];

}  // namespace switches
}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_AUTOFILL_SWITCHES_H_
