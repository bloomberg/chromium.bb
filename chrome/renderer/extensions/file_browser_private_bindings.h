// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_FILE_BROWSER_PRIVATE_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_FILE_BROWSER_PRIVATE_BINDINGS_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/renderer/extensions/chrome_v8_extension.h"

class FileBrowserPrivateBindings : public ChromeV8Extension {
 public:
  FileBrowserPrivateBindings();

 private:
  virtual v8::Handle<v8::FunctionTemplate> GetNativeFunction(
      v8::Handle<v8::String> name) OVERRIDE;

  DISALLOW_COPY_AND_ASSIGN(FileBrowserPrivateBindings);
};

#endif  // CHROME_RENDERER_EXTENSIONS_FILE_BROWSER_PRIVATE_BINDINGS_H_
