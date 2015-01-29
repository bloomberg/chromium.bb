// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_EXTENSION_VIEW_EXTENSION_VIEW_INTERNAL_API_H_
#define EXTENSIONS_BROWSER_API_EXTENSION_VIEW_EXTENSION_VIEW_INTERNAL_API_H_

#include "extensions/browser/api/capture_web_contents_function.h"
#include "extensions/browser/api/execute_code_function.h"
#include "extensions/browser/extension_function.h"
#include "extensions/browser/guest_view/extension_view/extension_view_guest.h"

namespace extensions {

// An abstract base class for async extensionview APIs. It does a process ID
// check in RunAsync, and then calls RunAsyncSafe which must be overriden by
// all subclasses.
class ExtensionViewInternalExtensionFunction : public AsyncExtensionFunction {
 public:
  ExtensionViewInternalExtensionFunction() {}

 protected:
  ~ExtensionViewInternalExtensionFunction() override {}

  // ExtensionFunction implementation.
  bool RunAsync() final;

 private:
  virtual bool RunAsyncSafe(ExtensionViewGuest* guest) = 0;
};

class ExtensionViewInternalNavigateFunction
    : public ExtensionViewInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("extensionViewInternal.navigate",
                             EXTENSIONVIEWINTERNAL_NAVIGATE);
  ExtensionViewInternalNavigateFunction() {}

 protected:
  ~ExtensionViewInternalNavigateFunction() override {}

 private:
  // ExtensionViewInternalExtensionFunction implementation.
  bool RunAsyncSafe(ExtensionViewGuest* guest) override;

  DISALLOW_COPY_AND_ASSIGN(ExtensionViewInternalNavigateFunction);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_EXTENSION_VIEW_EXTENSION_VIEW_INTERNAL_API_H_

