// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_BACKGROUND_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_BACKGROUND_VIEW_H_
#pragma once

#include <string>

#include "chrome/browser/chromeos/login/login_html_dialog.h"
#include "chrome/browser/chromeos/login/version_info_updater.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "chrome/browser/chromeos/status/status_area_view_chromeos.h"
#include "ui/views/view.h"

class DOMView;
class GURL;

namespace views {
class Label;
class Widget;
class WidgetDelegate;
}

namespace chromeos {

class ShutdownButton;

// View used to render the background during login. BackgroundView contains
// StatusAreaView.
class BackgroundView : public views::View,
                       public StatusAreaButton::Delegate,
                       public LoginHtmlDialog::Delegate,
                       public VersionInfoUpdater::Delegate {
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
  virtual ~BackgroundView();

  // Initializes the background view. It background_url is given (non empty),
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
  void CreateModalPopup(views::WidgetDelegate* view);

  // Gets the native window from the view widget.
  gfx::NativeWindow GetNativeWindow() const;

  // Toggles status area visibility.
  void SetStatusAreaVisible(bool visible);

  // Toggles whether status area is enabled.
  void SetStatusAreaEnabled(bool enable);

  // Shows screen saver.
  void ShowScreenSaver();

  // Hides screen saver.
  void HideScreenSaver();

  // Tells if screen saver is visible.
  bool IsScreenSaverVisible();

  // Tells if screen saver is enabled.
  bool ScreenSaverEnabled();

  // Sets default clock 24 hour mode to use.
  void SetDefaultUse24HourClock(bool use_24hour_clock);

  // Overridden from views::View:
  virtual void Layout() OVERRIDE;

 protected:
  // Overridden from views::View:
  virtual void ChildPreferredSizeChanged(View* child) OVERRIDE;
  virtual void OnLocaleChanged() OVERRIDE;

  // Overridden from StatusAreaButton::Delegate:
  virtual bool ShouldExecuteStatusAreaCommand(
      const views::View* button_view, int command_id) const OVERRIDE;
  virtual void ExecuteStatusAreaCommand(
      const views::View* button_view, int command_id) OVERRIDE;
  virtual gfx::Font GetStatusAreaFont(const gfx::Font& font) const OVERRIDE;
  virtual StatusAreaButton::TextStyle GetStatusAreaTextStyle() const OVERRIDE;
  virtual void ButtonVisibilityChanged(views::View* button_view) OVERRIDE;

  // Overridden from LoginHtmlDialog::Delegate:
  virtual void OnDialogClosed() OVERRIDE {}

  // Overridden from VersionInfoUpdater::Delegate:
  virtual void OnOSVersionLabelTextUpdated(
      const std::string& os_version_label_text) OVERRIDE;
  virtual void OnBootTimesLabelTextUpdated(
      const std::string& boot_times_label_text) OVERRIDE;

 private:
  // Creates and adds the status_area.
  void InitStatusArea();
  // Creates and adds the labels for version and boot time.
  void InitInfoLabels();

  // Invokes SetWindowType for the window. This is invoked during startup and
  // after we've painted.
  void UpdateWindowType();

  // All of these variables could be NULL.
  StatusAreaViewChromeos* status_area_;
  views::Label* os_version_label_;
  views::Label* boot_times_label_;
  ShutdownButton* shutdown_button_;

  // True if running official BUILD.
  bool is_official_build_;

  // DOMView for rendering a webpage as a background.
  DOMView* background_area_;

  // Proxy settings dialog that can be invoked from network menu.
  scoped_ptr<LoginHtmlDialog> proxy_settings_dialog_;

  // Updates version info.
  VersionInfoUpdater version_info_updater_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_BACKGROUND_VIEW_H_
