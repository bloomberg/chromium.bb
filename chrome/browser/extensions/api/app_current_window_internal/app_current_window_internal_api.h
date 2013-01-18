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
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.focus",
                             APP_CURRENTWINDOWINTERNAL_FOCUS)

 protected:
  virtual ~AppCurrentWindowInternalFocusFunction() {}
  virtual bool RunWithWindow(ShellWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalMaximizeFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.maximize",
                             APP_CURRENTWINDOWINTERNAL_MAXIMIZE)

 protected:
  virtual ~AppCurrentWindowInternalMaximizeFunction() {}
  virtual bool RunWithWindow(ShellWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalMinimizeFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.minimize",
                             APP_CURRENTWINDOWINTERNAL_MINIMIZE)

 protected:
  virtual ~AppCurrentWindowInternalMinimizeFunction() {}
  virtual bool RunWithWindow(ShellWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalRestoreFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.restore",
                             APP_CURRENTWINDOWINTERNAL_RESTORE)

 protected:
  virtual ~AppCurrentWindowInternalRestoreFunction() {}
  virtual bool RunWithWindow(ShellWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalDrawAttentionFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.drawAttention",
                             APP_CURRENTWINDOWINTERNAL_DRAWATTENTION)

 protected:
  virtual ~AppCurrentWindowInternalDrawAttentionFunction() {}
  virtual bool RunWithWindow(ShellWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalClearAttentionFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.clearAttention",
                             APP_CURRENTWINDOWINTERNAL_CLEARATTENTION)

 protected:
  virtual ~AppCurrentWindowInternalClearAttentionFunction() {}
  virtual bool RunWithWindow(ShellWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalShowFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.show",
                             APP_CURRENTWINDOWINTERNAL_SHOW)

 protected:
  virtual ~AppCurrentWindowInternalShowFunction() {}
  virtual bool RunWithWindow(ShellWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalHideFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.hide",
                             APP_CURRENTWINDOWINTERNAL_HIDE)

 protected:
  virtual ~AppCurrentWindowInternalHideFunction() {}
  virtual bool RunWithWindow(ShellWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalSetBoundsFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.setBounds",
                             APP_CURRENTWINDOWINTERNAL_SETBOUNDS)
 protected:
  virtual ~AppCurrentWindowInternalSetBoundsFunction() {}
  virtual bool RunWithWindow(ShellWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalSetIconFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.setIcon",
                             APP_CURRENTWINDOWINTERNAL_SETICON)

 protected:
  virtual ~AppCurrentWindowInternalSetIconFunction() {}
  virtual bool RunWithWindow(ShellWindow* window) OVERRIDE;
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_APP_CURRENT_WINDOW_INTERNAL_APP_CURRENT_WINDOW_INTERNAL_API_H_
