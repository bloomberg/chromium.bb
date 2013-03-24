// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_UI_APP_LAUNCH_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_UI_APP_LAUNCH_VIEW_H_

#include <string>

#include "ash/ash_export.h"
#include "base/callback.h"
#include "content/public/browser/web_contents_observer.h"
#include "googleurl/src/gurl.h"
#include "ui/views/widget/widget_delegate.h"

namespace content {
class BrowserContent;
}

namespace views {
class WebView;
}

namespace chromeos {

class AppLaunchUI;

enum AppLaunchState {
  APP_LAUNCH_STATE_PREPARING_NETWORK,
  APP_LAUNCH_STATE_INSTALLING_APPLICATION,
};

void ShowAppLaunchSplashScreen(const std::string& app_id);
void CloseAppLaunchSplashScreen();
void UpdateAppLaunchSplashScreenState(AppLaunchState state);

namespace internal {

// Shows application launch/install splash screen in exclusive app mode (kiosk).
class AppLaunchView : public views::WidgetDelegateView,
                      public content::WebContentsObserver {
 public:
  static void ShowAppLaunchSplashScreen(const std::string& app_id);
  static void CloseAppLaunchSplashScreen();
  static void UpdateAppLaunchState(AppLaunchState state);

 private:
  explicit AppLaunchView(const std::string& app_id);
  virtual ~AppLaunchView();

  // views::WidgetDelegate overrides.
  virtual views::View* GetContentsView() OVERRIDE;

  // content::WebContentsObserver overrides.
  virtual void RenderViewGone(base::TerminationStatus status) OVERRIDE;

  void Show();
  void Close();

  // Updates UI state of the app launch splash screen.
  void UpdateState(AppLaunchState state);

  // Creates and adds web contents to our view.
  void AddChildWebContents();

  // Loads the splash screen in the WebView's webcontent. If the webcontents
  // don't exist, they'll be created by WebView.
  void LoadSplashScreen();

  // Initializes container window.
  void InitializeWindow();

  // Creates and shows a frameless full screen window containing our view.
  void ShowWindow();

  // Host for the extension that implements this dialog.
  views::WebView* app_launch_webview_;

  // Window that holds the webview.
  views::Widget* container_window_;

  const std::string app_id_;

  // Launch state.
  AppLaunchState state_;

  AppLaunchUI* app_launch_ui_;  // Not owned.

  DISALLOW_COPY_AND_ASSIGN(AppLaunchView);
};

}  // namespace internal

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_UI_APP_LAUNCH_VIEW_H_
