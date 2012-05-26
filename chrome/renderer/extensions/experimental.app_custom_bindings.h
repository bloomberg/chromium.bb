// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_EXPERIMENTAL_APP_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_EXPERIMENTAL_APP_CUSTOM_BINDINGS_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"

namespace extensions {

// Custom bindings for the nativeFileSystem API.
class ExperimentalAppCustomBindings : public ChromeV8Extension {
 public:
  ExperimentalAppCustomBindings();

 private:
  DISALLOW_COPY_AND_ASSIGN(ExperimentalAppCustomBindings);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_EXPERIMENTAL_APP_CUSTOM_BINDINGS_H_
