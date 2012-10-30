// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_ACCESSIBILITY_TEST_UTILS_WIN_H_
#define CONTENT_PUBLIC_TEST_ACCESSIBILITY_TEST_UTILS_WIN_H_

#include <vector>

#include "base/basictypes.h"
#include "base/string16.h"

namespace content {

string16 IAccessibleRoleToString(int32 ia_role);
string16 IAccessible2RoleToString(int32 ia_role);
string16 IAccessibleStateToString(int32 ia_state);
void IAccessibleStateToStringVector(int32 ia_state,
                                    std::vector<string16>* result);
string16 IAccessible2StateToString(int32 ia2_state);
void IAccessible2StateToStringVector(int32 ia_state,
                                     std::vector<string16>* result);

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_ACCESSIBILITY_TEST_UTILS_WIN_H_
