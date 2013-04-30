// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_COMMON_FORMS_SEEN_PARAM_H_
#define COMPONENTS_AUTOFILL_COMMON_FORMS_SEEN_PARAM_H_

namespace autofill {

// Specifies the nature of the forms triggering the call.
enum FormsSeenState {
  // Forms present on page load with no additional unsent forms.
  NO_SPECIAL_FORMS_SEEN = 0,
  // If this an Autocheckout page, there are more that should be checked.
  PARTIAL_FORMS_SEEN = 1,
  // Forms were added dynamically post-page load.
  DYNAMIC_FORMS_SEEN = 2,
};

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_COMMON_FORMS_SEEN_PARAM_H_
