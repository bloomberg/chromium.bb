// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SESSION_SESSION_TYPES_H_
#define ASH_SESSION_SESSION_TYPES_H_

namespace ash {

// The index of the user profile to use. The list is always LRU sorted so that
// index 0 is the currently active user.
using UserIndex = int;

}  // namespace ash

#endif  // ASH_SESSION_SESSION_TYPES_H_
