// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_COMMON_AUTOFILL_SWITCHES_H_
#define COMPONENTS_AUTOFILL_COMMON_AUTOFILL_SWITCHES_H_

namespace switches {

// All switches in alphabetical order. The switches should be documented
// alongside the definition of their values in the .cc file.
extern const char kAutofillServiceUrl[];
extern const char kBypassAutocheckoutWhitelist[];
extern const char kEnableExperimentalFormFilling[];
extern const char kShowAutofillTypePredictions[];
extern const char kWalletSecureServiceUrl[];
extern const char kWalletServiceUrl[];
extern const char kWalletServiceUseProd[];

}  // namespace switches

#endif  // COMPONENTS_AUTOFILL_COMMON_AUTOFILL_SWITCHES_H_
