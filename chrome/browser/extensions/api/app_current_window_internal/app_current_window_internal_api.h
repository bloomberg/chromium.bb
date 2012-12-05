// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_APP_CURRENT_WINDOW_INTERNAL_APP_CURRENT_WINDOW_INTERNAL_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_APP_CURRENT_WINDOW_INTERNAL_APP_CURRENT_WINDOW_INTERNAL_API_H_

#include "chrome/browser/extensions/extension_function.h"

class ShellWindow;

namespace extensions {

class AppCurrentWindowInternalExtensionFunction : public SyncExtensionFunction {
 protected:
  virtual ~AppCurrentWindowInternalExtensionFunction() {}

  // Invoked with the current shell window.
  virtual bool RunWithWindow(ShellWindow* window) = 0;

 private:
  virtual bool RunImpl() OVERRIDE;
};

class AppCurrentWindowInternalFocusFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("app.currentWindowInternal.focus");

 protected:
  virtual ~AppCurrentWindowInternalFocusFunction() {}
  virtual bool RunWithWindow(ShellWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalMaximizeFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("app.currentWindowInternal.maximize");

 protected:
  virtual ~AppCurrentWindowInternalMaximizeFunction() {}
  virtual bool RunWithWindow(ShellWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalMinimizeFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("app.currentWindowInternal.minimize");

 protected:
  virtual ~AppCurrentWindowInternalMinimizeFunction() {}
  virtual bool RunWithWindow(ShellWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalRestoreFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("app.currentWindowInternal.restore");

 protected:
  virtual ~AppCurrentWindowInternalRestoreFunction() {}
  virtual bool RunWithWindow(ShellWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalDrawAttentionFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("app.currentWindowInternal.drawAttention");

 protected:
  virtual ~AppCurrentWindowInternalDrawAttentionFunction() {}
  virtual bool RunWithWindow(ShellWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalClearAttentionFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("app.currentWindowInternal.clearAttention");

 protected:
  virtual ~AppCurrentWindowInternalClearAttentionFunction() {}
  virtual bool RunWithWindow(ShellWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalShowFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("app.currentWindowInternal.show");

 protected:
  virtual ~AppCurrentWindowInternalShowFunction() {}
  virtual bool RunWithWindow(ShellWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalHideFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("app.currentWindowInternal.hide");

 protected:
  virtual ~AppCurrentWindowInternalHideFunction() {}
  virtual bool RunWithWindow(ShellWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalSetBoundsFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("app.currentWindowInternal.setBounds");
 protected:
  virtual ~AppCurrentWindowInternalSetBoundsFunction() {}
  virtual bool RunWithWindow(ShellWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalSetIconFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION_NAME("app.currentWindowInternal.setIcon");

 protected:
  virtual ~AppCurrentWindowInternalSetIconFunction() {}
  virtual bool RunWithWindow(ShellWindow* window) OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_APP_CURRENT_WINDOW_INTERNAL_APP_CURRENT_WINDOW_INTERNAL_API_H_
