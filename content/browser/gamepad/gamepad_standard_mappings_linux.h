// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_GAMEPAD_GAMEPAD_STANDARD_MAPPINGS_LINUX_H_
#define CONTENT_BROWSER_GAMEPAD_GAMEPAD_STANDARD_MAPPINGS_LINUX_H_

#include "base/string_piece.h"

namespace WebKit {
class WebGamepad;
}

namespace content {

typedef void (*GamepadStandardMappingFunction)(
    const WebKit::WebGamepad& original,
    WebKit::WebGamepad* mapped);

GamepadStandardMappingFunction GetGamepadStandardMappingFunction(
    const base::StringPiece& vendor_id,
    const base::StringPiece& product_id);

}  // namespace content

#endif  // CONTENT_BROWSER_GAMEPAD_GAMEPAD_STANDARD_MAPPINGS_LINUX_H_
