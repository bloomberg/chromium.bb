// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Functions that are shared between the Identity Service implementation and its
// consumers. Currently in //components/signin because they are used by classes
// in this component, which cannot depend on //services/identity to avoid a
// dependency cycle. When these classes have no direct consumers and are moved
// to //services/identity, these functions should correspondingly be moved to
// //services/identity/public/cpp.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_IDENTITY_UTILS_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_IDENTITY_UTILS_H_

#include "base/strings/string_piece.h"

namespace identity {

// Returns true if the username is allowed based on the pattern string.
//
// NOTE: Can be moved to //services/identity/public/cpp once SigninManager is
// moved to //services/identity.
bool IsUsernameAllowedByPattern(base::StringPiece username,
                                base::StringPiece pattern);
}  // namespace identity

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_IDENTITY_UTILS_H_
