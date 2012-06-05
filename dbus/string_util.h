// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_STRING_UTIL_H_
#define DBUS_STRING_UTIL_H_
#pragma once

#include <string>

namespace dbus {

// Returns true if the specified string is a valid object path.
bool IsValidObjectPath(const std::string& value);

}  // namespace dbus

#endif  // DBUS_STRING_UTIL_H_
