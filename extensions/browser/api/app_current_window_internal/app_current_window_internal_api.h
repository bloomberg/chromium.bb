// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef EXTENSIONS_BROWSER_API_APP_CURRENT_WINDOW_INTERNAL_APP_CURRENT_WINDOW_INTERNAL_API_H_
#define EXTENSIONS_BROWSER_API_APP_CURRENT_WINDOW_INTERNAL_APP_CURRENT_WINDOW_INTERNAL_API_H_

#include "extensions/browser/extension_function.h"

namespace extensions {

class AppWindow;

class AppCurrentWindowInternalExtensionFunction : public SyncExtensionFunction {
 protected:
  virtual ~AppCurrentWindowInternalExtensionFunction() {}

  // Invoked with the current app window.
  virtual bool RunWithWindow(AppWindow* window) = 0;

 private:
  virtual bool RunSync() OVERRIDE;
};

class AppCurrentWindowInternalFocusFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.focus",
                             APP_CURRENTWINDOWINTERNAL_FOCUS)

 protected:
  virtual ~AppCurrentWindowInternalFocusFunction() {}
  virtual bool RunWithWindow(AppWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalFullscreenFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.fullscreen",
                             APP_CURRENTWINDOWINTERNAL_FULLSCREEN)

 protected:
  virtual ~AppCurrentWindowInternalFullscreenFunction() {}
  virtual bool RunWithWindow(AppWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalMaximizeFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.maximize",
                             APP_CURRENTWINDOWINTERNAL_MAXIMIZE)

 protected:
  virtual ~AppCurrentWindowInternalMaximizeFunction() {}
  virtual bool RunWithWindow(AppWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalMinimizeFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.minimize",
                             APP_CURRENTWINDOWINTERNAL_MINIMIZE)

 protected:
  virtual ~AppCurrentWindowInternalMinimizeFunction() {}
  virtual bool RunWithWindow(AppWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalRestoreFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.restore",
                             APP_CURRENTWINDOWINTERNAL_RESTORE)

 protected:
  virtual ~AppCurrentWindowInternalRestoreFunction() {}
  virtual bool RunWithWindow(AppWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalDrawAttentionFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.drawAttention",
                             APP_CURRENTWINDOWINTERNAL_DRAWATTENTION)

 protected:
  virtual ~AppCurrentWindowInternalDrawAttentionFunction() {}
  virtual bool RunWithWindow(AppWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalClearAttentionFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.clearAttention",
                             APP_CURRENTWINDOWINTERNAL_CLEARATTENTION)

 protected:
  virtual ~AppCurrentWindowInternalClearAttentionFunction() {}
  virtual bool RunWithWindow(AppWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalShowFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.show",
                             APP_CURRENTWINDOWINTERNAL_SHOW)

 protected:
  virtual ~AppCurrentWindowInternalShowFunction() {}
  virtual bool RunWithWindow(AppWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalHideFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.hide",
                             APP_CURRENTWINDOWINTERNAL_HIDE)

 protected:
  virtual ~AppCurrentWindowInternalHideFunction() {}
  virtual bool RunWithWindow(AppWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalSetBoundsFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.setBounds",
                             APP_CURRENTWINDOWINTERNAL_SETBOUNDS)
 protected:
  virtual ~AppCurrentWindowInternalSetBoundsFunction() {}
  virtual bool RunWithWindow(AppWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalSetSizeConstraintsFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.setSizeConstraints",
                             APP_CURRENTWINDOWINTERNAL_SETSIZECONSTRAINTS)
 protected:
  virtual ~AppCurrentWindowInternalSetSizeConstraintsFunction() {}
  virtual bool RunWithWindow(AppWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalSetIconFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.setIcon",
                             APP_CURRENTWINDOWINTERNAL_SETICON)

 protected:
  virtual ~AppCurrentWindowInternalSetIconFunction() {}
  virtual bool RunWithWindow(AppWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalSetBadgeIconFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.setBadgeIcon",
                             APP_CURRENTWINDOWINTERNAL_SETBADGEICON)

 protected:
  virtual ~AppCurrentWindowInternalSetBadgeIconFunction() {}
  virtual bool RunWithWindow(AppWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalClearBadgeFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.clearBadge",
                             APP_CURRENTWINDOWINTERNAL_CLEARBADGE)

 protected:
  virtual ~AppCurrentWindowInternalClearBadgeFunction() {}
  virtual bool RunWithWindow(AppWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalSetShapeFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.setShape",
                             APP_CURRENTWINDOWINTERNAL_SETSHAPE)

 protected:
  virtual ~AppCurrentWindowInternalSetShapeFunction() {}
  virtual bool RunWithWindow(AppWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalSetAlwaysOnTopFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION("app.currentWindowInternal.setAlwaysOnTop",
                             APP_CURRENTWINDOWINTERNAL_SETALWAYSONTOP)

 protected:
  virtual ~AppCurrentWindowInternalSetAlwaysOnTopFunction() {}
  virtual bool RunWithWindow(AppWindow* window) OVERRIDE;
};

class AppCurrentWindowInternalSetVisibleOnAllWorkspacesFunction
    : public AppCurrentWindowInternalExtensionFunction {
 public:
  DECLARE_EXTENSION_FUNCTION(
      "app.currentWindowInternal.setVisibleOnAllWorkspaces",
      APP_CURRENTWINDOWINTERNAL_SETVISIBLEONALLWORKSPACES)

 protected:
  virtual ~AppCurrentWindowInternalSetVisibleOnAllWorkspacesFunction() {}
  virtual bool RunWithWindow(AppWindow* window) OVERRIDE;
};

}  // namespace extensions

#endif  // EXTENSIONS_BROWSER_API_APP_CURRENT_WINDOW_INTERNAL_APP_CURRENT_WINDOW_INTERNAL_API_H_
