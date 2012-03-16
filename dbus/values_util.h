// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef DBUS_VALUES_UTIL_H_
#define DBUS_VALUES_UTIL_H_
#pragma once

namespace base {
class Value;
}

namespace dbus {

class MessageReader;

// Pops a value from |reader| as a base::Value.
// Returns NULL if an error occurs.
// Note: Integer values larger than int32 (including uint32) are converted to
// double.  Non-string diciontary keys are converted to strings.
base::Value* PopDataAsValue(MessageReader* reader);

}  // namespace dbus

#endif  // DBUS_VALUES_UTIL_H_
