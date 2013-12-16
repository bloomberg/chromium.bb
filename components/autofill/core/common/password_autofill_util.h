// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_AUTOFILL_UTIL_H_
#define COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_AUTOFILL_UTIL_H_

namespace autofill {

// Returns true if the user has specified the flag to ignore autocomplete='off'
// for password fields in the password manager.
bool ShouldIgnoreAutocompleteOffForPasswordFields();

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CORE_COMMON_PASSWORD_AUTOFILL_UTIL_H_
