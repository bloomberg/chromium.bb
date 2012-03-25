// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DBUS_INTROSPECT_UTIL_H_
#define CHROME_BROWSER_CHROMEOS_DBUS_INTROSPECT_UTIL_H_
#pragma once

#include <string>
#include <vector>

namespace chromeos {

// Parses XML-formatted introspection data returned by
// org.freedesktop.DBus.Introspectable.Introspect and returns the list of
// interface names declared within.
std::vector<std::string> GetInterfacesFromIntrospectResult(
    const std::string& xml_data);

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_DBUS_INTROSPECT_UTIL_H_
