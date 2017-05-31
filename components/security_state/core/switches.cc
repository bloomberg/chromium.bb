// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/security_state/core/switches.h"

namespace security_state {
namespace switches {

// Use to opt-in to marking HTTP as non-secure.
const char kMarkHttpAs[] = "mark-non-secure-as";
const char kMarkHttpAsDangerous[] = "non-secure";
const char kMarkHttpAsNonSecureAfterEditing[] = "non-secure-after-editing";
const char kMarkHttpAsNonSecureWhileIncognito[] = "non-secure-while-incognito";
const char kMarkHttpAsNonSecureWhileIncognitoOrEditing[] =
    "non-secure-while-incognito-or-editing";
}  // namespace switches
}  // namespace security_state
