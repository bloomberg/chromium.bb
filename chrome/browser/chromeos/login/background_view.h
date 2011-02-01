// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_BACKGROUND_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_BACKGROUND_VIEW_H_
#pragma once

#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/login/login_html_dialog.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "chrome/browser/chromeos/version_loader.h"
#include "views/view.h"

namespace views {
class Label;
class TextButton;
class Widget;
class WindowDelegate;
}

class DOMView;
class GURL;
class Profile;

namespace chromeos {

class OobeProgressBar;
class ShutdownButton;
class StatusAreaView;

// View used to render the background during login. BackgroundView contains
// StatusAreaView.
class BackgroundView : public views::View,
                       public StatusAreaHost,
                       public chromeos::LoginHtmlDialog::Delegate {
 public:
  enum LoginStep {
    SELECT_NETWORK,
    EULA,
    SIGNIN,
    REGISTRATION,
    PICTURE,

    // Steps count, must be the last in the enum.
    STEPS_COUNT
  };

  BackgroundView();

  // Initializes the background view. It backgroun_url is given (non empty),
  // it creates a DOMView background area that renders a webpage.
  void Init(const GURL& background_url);

  // Enable/disable shutdown button.
  void EnableShutdownButton(bool enable);

  // Creates a window containing an instance of WizardContentsView as the root
  // view. The caller is responsible for showing (and closing) the returned
  // widget. The BackgroundView is set in |view|. If background_url is non
  // empty, the content page of the url is displayed as a background.
  static views::Widget* CreateWindowContainingView(
      const gfx::Rect& bounds,
      const GURL& background_url,
      BackgroundView** view);

  // Create a modal popup view.
  void CreateModalPopup(views::WindowDelegate* view);

  // Overridden from StatusAreaHost:
  virtual gfx::NativeWindow GetNativeWindow() const;

  // Toggles status area visibility.
  void SetStatusAreaVisible(bool visible);

  // Toggles whether status area is enabled.
  void SetStatusAreaEnabled(bool enable);

  // Toggles OOBE progress bar visibility, the bar is hidden by default.
  void SetOobeProgressBarVisible(bool visible);

  // Gets progress bar visibility.
  bool IsOobeProgressBarVisible();

  // Sets current step on OOBE progress bar.
  void SetOobeProgress(LoginStep step);

  // Shows screen saver.
  void ShowScreenSaver();

  // Hides screen saver.
  void HideScreenSaver();

  // Tells if screen saver is visible.
  bool IsScreenSaverVisible();

  // Tells if screen saver is enabled.
  bool ScreenSaverEnabled();

 protected:
  // Overridden from views::View:
  virtual void Paint(gfx::Canvas* canvas);
  virtual void Layout();
  virtual void ChildPreferredSizeChanged(View* child);
  virtual void OnLocaleChanged();

  // Overridden from StatusAreaHost:
  virtual Profile* GetProfile() const { return NULL; }
  virtual void ExecuteBrowserCommand(int id) const {}
  virtual bool ShouldOpenButtonOptions(
      const views::View* button_view) const;
  virtual void OpenButtonOptions(const views::View* button_view);
  virtual ScreenMode GetScreenMode() const;

  // Overridden from LoginHtmlDialog::Delegate:
  virtual void OnDialogClosed() {}

 private:
  // Creates and adds the status_area.
  void InitStatusArea();
  // Creates and adds the labels for version and boot time.
  void InitInfoLabels();
  // Creates and add OOBE progress bar.
  void InitProgressBar();

  // Invokes SetWindowType for the window. This is invoked during startup and
  // after we've painted.
  void UpdateWindowType();

  // Callback from chromeos::VersionLoader giving the version.
  void OnVersion(VersionLoader::Handle handle, std::string version);
  // Callback from chromeos::InfoLoader giving the boot times.
  void OnBootTimes(
      BootTimesLoader::Handle handle, BootTimesLoader::BootTimes boot_times);

  // All of these variables could be NULL.
  StatusAreaView* status_area_;
  views::Label* os_version_label_;
  views::Label* boot_times_label_;
  OobeProgressBar* progress_bar_;
  ShutdownButton* shutdown_button_;

  // Handles asynchronously loading the version.
  VersionLoader version_loader_;
  // Used to request the version.
  CancelableRequestConsumer version_consumer_;

  // Handles asynchronously loading the boot times.
  BootTimesLoader boot_times_loader_;
  // Used to request the boot times.
  CancelableRequestConsumer boot_times_consumer_;

  // Has Paint been invoked once? The value of this is passed to the window
  // manager.
  // TODO(sky): nuke this when the wm knows when chrome has painted.
  bool did_paint_;

  // True if running official BUILD.
  bool is_official_build_;

  // DOMView for rendering a webpage as a background.
  DOMView* background_area_;

  // Proxy settings dialog that can be invoked from network menu.
  scoped_ptr<LoginHtmlDialog> proxy_settings_dialog_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_BACKGROUND_VIEW_H_
