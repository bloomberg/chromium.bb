// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EXPERIMENTAL_USB_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_EXPERIMENTAL_USB_CUSTOM_BINDINGS_H_
#pragma once

#include "chrome/renderer/extensions/chrome_v8_extension.h"

namespace extensions {

class ExperimentalUsbCustomBindings : public ChromeV8Extension {
 public:
  ExperimentalUsbCustomBindings();
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_EXPERIMENTAL_USB_CUSTOM_BINDINGS_H_
