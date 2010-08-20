// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_KEYBOARD_CODE_CONVERSION_H_
#define BASE_KEYBOARD_CODE_CONVERSION_H_
#pragma once

#include "base/keyboard_codes.h"

#include <string>

namespace base {

// Convert a KeyIdentifer (see Section 6.3.3 here:
// http://www.w3.org/TR/DOM-Level-3-Events/#keyset-keyidentifiers)
// to a base::KeyboardCode.
KeyboardCode KeyCodeFromKeyIdentifier(const std::string& key_identifier);

}  // namespace

#endif  // BASE_KEYBOARD_CODE_CONVERSION_H_
