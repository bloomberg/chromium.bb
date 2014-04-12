// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_FILE_SYSTEM_NATIVES_H_
#define CHROME_RENDERER_EXTENSIONS_FILE_SYSTEM_NATIVES_H_

#include "base/compiler_specific.h"
#include "extensions/renderer/object_backed_native_handler.h"

namespace extensions {
class ChromeV8Context;

// Custom bindings for the nativeFileSystem API.
class FileSystemNatives : public ObjectBackedNativeHandler {
 public:
  explicit FileSystemNatives(ChromeV8Context* context);

 private:
  void GetFileEntry(const v8::FunctionCallbackInfo<v8::Value>& args);
  void GetIsolatedFileSystem(const v8::FunctionCallbackInfo<v8::Value>& args);
  void CrackIsolatedFileSystemName(
      const v8::FunctionCallbackInfo<v8::Value>& args);
  // Constructs a DOMError object to be used in JavaScript.
  void GetDOMError(const v8::FunctionCallbackInfo<v8::Value>& args);

  DISALLOW_COPY_AND_ASSIGN(FileSystemNatives);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_FILE_SYSTEM_NATIVES_H_
