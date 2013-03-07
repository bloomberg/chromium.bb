// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_RENDERER_EXTENSIONS_FILE_SYSTEM_NATIVES_H_
#define CHROME_RENDERER_EXTENSIONS_FILE_SYSTEM_NATIVES_H_

#include "base/compiler_specific.h"
#include "chrome/renderer/extensions/object_backed_native_handler.h"

namespace extensions {

// Custom bindings for the nativeFileSystem API.
class FileSystemNatives : public ObjectBackedNativeHandler {
 public:
  explicit FileSystemNatives(v8::Handle<v8::Context> context);

 private:
  v8::Handle<v8::Value> GetFileEntry(const v8::Arguments& args);
  v8::Handle<v8::Value> GetIsolatedFileSystem(const v8::Arguments& args);

  DISALLOW_COPY_AND_ASSIGN(FileSystemNatives);
};

}  // namespace extensions

#endif  // CHROME_RENDERER_EXTENSIONS_FILE_SYSTEM_NATIVES_H_
