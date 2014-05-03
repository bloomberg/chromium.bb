// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_SYSTEM_MEMORY_SYSTEM_MEMORY_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_SYSTEM_MEMORY_SYSTEM_MEMORY_API_H_

#include "chrome/common/extensions/api/system_memory.h"
#include "extensions/browser/extension_function.h"

namespace extensions {

class SystemMemoryGetInfoFunction : public AsyncExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("system.memory.getInfo", SYSTEM_MEMORY_GETINFO)
  SystemMemoryGetInfoFunction();

 private:
  virtual ~SystemMemoryGetInfoFunction();
  virtual bool RunAsync() OVERRIDE;
  void OnGetMemoryInfoCompleted(bool success);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_SYSTEM_MEMORY_SYSTEM_MEMORY_API_H_
