// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_BLUETOOTH_BLUETOOTH_ERROR_H_
#define CONTENT_COMMON_BLUETOOTH_BLUETOOTH_ERROR_H_

namespace content {

// Error enumerations corresponding to blink::WebBluetoothError::ErrorType
// used to create DOMExceptions.
enum class BluetoothError { NOT_FOUND, SECURITY, ENUM_MAX_VALUE = SECURITY };

}  // namespace content

#endif  // CONTENT_COMMON_BLUETOOTH_BLUETOOTH_ERROR_H_
