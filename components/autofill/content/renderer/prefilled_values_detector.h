// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_AUTOFILL_CONTENT_RENDERER_PREFILLED_VALUES_DETECTOR_H_
#define COMPONENTS_AUTOFILL_CONTENT_RENDERER_PREFILLED_VALUES_DETECTOR_H_

#include <string>

#include "base/containers/flat_set.h"

namespace autofill {

// Returns a set of known username placeholders, all guaranteed to be lower
// case.
// This is only exposed for testing.
const base::flat_set<std::string, std::less<>>& KnownUsernamePlaceholders();

// Checks if the prefilled value of the username element is one of the known
// values possibly used as placeholders. The list of possible placeholder
// values comes from popular sites exhibiting this issue.
//
// If |username_value| is in KnownUsernamePlaceholder(), the password manager
// takes the liberty to override the contents of the username field.
//
// TODO(crbug.com/832622): Remove this once a stable solution is in place.
bool PossiblePrefilledUsernameValue(const std::string& username_value);

}  // namespace autofill

#endif  // COMPONENTS_AUTOFILL_CONTENT_RENDERER_PREFILLED_VALUES_DETECTOR_H_
