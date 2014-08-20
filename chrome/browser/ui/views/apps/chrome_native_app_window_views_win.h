// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_WIN_H_
#define CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_WIN_H_

#include "chrome/browser/ui/views/apps/chrome_native_app_window_views.h"

namespace web_app {
struct ShortcutInfo;
}

class GlassAppWindowFrameViewWin;

// Windows-specific parts of the views-backed native shell window implementation
// for packaged apps.
class ChromeNativeAppWindowViewsWin : public ChromeNativeAppWindowViews {
 public:
  ChromeNativeAppWindowViewsWin();

  GlassAppWindowFrameViewWin* glass_frame_view() {
    return glass_frame_view_;
  }

 private:
  void ActivateParentDesktopIfNecessary();

  void OnShortcutInfoLoaded(
      const web_app::ShortcutInfo& shortcut_info);

  HWND GetNativeAppWindowHWND() const;
  bool IsRunningInAsh();
  void EnsureCaptionStyleSet();

  // Overridden from ChromeNativeAppWindowViews:
  virtual void OnBeforeWidgetInit(views::Widget::InitParams* init_params,
                                  views::Widget* widget) OVERRIDE;
  virtual void InitializeDefaultWindow(
      const apps::AppWindow::CreateParams& create_params) OVERRIDE;
  virtual views::NonClientFrameView* CreateStandardDesktopAppFrame() OVERRIDE;

  // Overridden from ui::BaseWindow:
  virtual void Show() OVERRIDE;
  virtual void Activate() OVERRIDE;

  // Overridden from extensions::NativeAppWindow:
  virtual void UpdateShelfMenu() OVERRIDE;

  base::WeakPtrFactory<ChromeNativeAppWindowViewsWin> weak_ptr_factory_;

  // Populated if there is a glass frame, which provides special information
  // to the native widget implementation. This will be NULL if there is no
  // glass frame. Note, this can change from NULL to non-NULL and back again
  // throughout the life of a window, e.g. if DWM is enabled and disabled.
  GlassAppWindowFrameViewWin* glass_frame_view_;

  // The Windows Application User Model ID identifying the app.
  base::string16 app_model_id_;

  DISALLOW_COPY_AND_ASSIGN(ChromeNativeAppWindowViewsWin);
};

#endif  // CHROME_BROWSER_UI_VIEWS_APPS_CHROME_NATIVE_APP_WINDOW_VIEWS_WIN_H_
