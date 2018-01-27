// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_FILE_MANAGER_PRIVATE_CUSTOM_BINDINGS_H_
#define CHROME_RENDERER_EXTENSIONS_FILE_MANAGER_PRIVATE_CUSTOM_BINDINGS_H_

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "extensions/renderer/object_backed_native_handler.h"

namespace extensions {

// Custom bindings for the fileManagerPrivate API.
class FileManagerPrivateCustomBindings : public ObjectBackedNativeHandler {
 public:
  explicit FileManagerPrivateCustomBindings(ScriptContext* context);

  // ObjectBackedNativeHandler:
  void AddRoutes() override;

 private:
  void GetFileSystem(const v8::FunctionCallbackInfo<v8::Value>& args);
  void GetExternalFileEntry(const v8::FunctionCallbackInfo<v8::Value>& args);
  void GetEntryURL(const v8::FunctionCallbackInfo<v8::Value>& args);

  DISALLOW_COPY_AND_ASSIGN(FileManagerPrivateCustomBindings);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_FILE_MANAGER_PRIVATE_CUSTOM_BINDINGS_H_
