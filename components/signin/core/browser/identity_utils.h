// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// Collection of utility functions to support migrating different consumers of
// the SigninManager component to the Identity service, which will eventually be
// migrated to //services/identity once the SigninManager class gets removed.

#ifndef COMPONENTS_SIGNIN_CORE_BROWSER_IDENTITY_UTILS_H_
#define COMPONENTS_SIGNIN_CORE_BROWSER_IDENTITY_UTILS_H_

#include "base/strings/string_piece.h"

namespace identity {

// Returns true if the username is allowed based on the pattern string.
bool IsUsernameAllowedByPattern(base::StringPiece username,
                                base::StringPiece pattern);
}  // namespace identity

#endif  // COMPONENTS_SIGNIN_CORE_BROWSER_IDENTITY_UTILS_H_
