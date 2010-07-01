// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_GTK_UTIL_H_
#define BASE_GTK_UTIL_H_

#include <string>

namespace gtk_util {

// Change windows accelerator style to GTK style. (GTK uses _ for
// accelerators.  Windows uses & with && as an escape for &.)
std::string ConvertAcceleratorsFromWindowsStyle(const std::string& label);

// Removes the "&" accelerators from a Windows label.
std::string RemoveWindowsStyleAccelerators(const std::string& label);

}  // namespace gtk_util

#endif  // BASE_GTK_UTIL_H_
