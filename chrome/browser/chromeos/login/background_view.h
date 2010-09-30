// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_BACKGROUND_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_BACKGROUND_VIEW_H_
#pragma once

#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "chrome/browser/chromeos/version_loader.h"
#include "views/controls/button/button.h"
#include "views/view.h"

namespace views {
class Widget;
class Label;
class TextButton;
}

class DOMView;
class GURL;
class Profile;

namespace chromeos {

class OobeProgressBar;
class StatusAreaView;

// View used to render the background during login. BackgroundView contains
// StatusAreaView.
class BackgroundView : public views::View,
                       public StatusAreaHost,
                       public views::ButtonListener {
 public:
  // Delegate class to handle notificatoin from the view.
  class Delegate {
   public:
    virtual ~Delegate() {}

    // Initializes incognito login.
    virtual void OnGoIncognitoButton() = 0;
  };

  enum LoginStep {
    SELECT_NETWORK,
#if defined(OFFICIAL_BUILD)
    EULA,
#endif
    SIGNIN,
#if defined(OFFICIAL_BUILD)
    REGISTRATION,
#endif
    PICTURE
  };

  BackgroundView();

  // Initializes the background view. It backgroun_url is given (non empty),
  // it creates a DOMView background area that renders a webpage.
  void Init(const GURL& background_url);

  // Creates a window containing an instance of WizardContentsView as the root
  // view. The caller is responsible for showing (and closing) the returned
  // widget. The BackgroundView is set in |view|. If background_url is non
  // empty, the content page of the url is displayed as a background.
  static views::Widget* CreateWindowContainingView(
      const gfx::Rect& bounds,
      const GURL& background_url,
      BackgroundView** view);

  // Toggles status area visibility.
  void SetStatusAreaVisible(bool visible);

  // Toggles OOBE progress bar visibility, the bar is hidden by default.
  void SetOobeProgressBarVisible(bool visible);

  // Gets progress bar visibility.
  bool IsOobeProgressBarVisible();

  // Sets current step on OOBE progress bar.
  void SetOobeProgress(LoginStep step);

  // Toggles GoIncognito button visibility.
  void SetGoIncognitoButtonVisible(bool visible, Delegate *delegate);

  // Shows screen saver.
  void ShowScreenSaver();

  // Hides screen saver.
  void HideScreenSaver();

  // Tells if screen saver is visible.
  bool IsScreenSaverVisible();

  // Tells if screen saver is enabled.
  bool ScreenSaverEnabled();

  // Tells that owner has been changed.
  void OnOwnerChanged();

 protected:
  // Overridden from views::View:
  virtual void Paint(gfx::Canvas* canvas);
  virtual void Layout();
  virtual void ChildPreferredSizeChanged(View* child);
  virtual void OnLocaleChanged();

  // Overridden from StatusAreaHost:
  virtual Profile* GetProfile() const { return NULL; }
  virtual gfx::NativeWindow GetNativeWindow() const;
  virtual void ExecuteBrowserCommand(int id) const {}
  virtual bool ShouldOpenButtonOptions(
      const views::View* button_view) const;
  virtual void OpenButtonOptions(const views::View* button_view) const;
  virtual bool IsBrowserMode() const;
  virtual bool IsScreenLockerMode() const;

  // Overridden from views::ButtonListener.
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

 private:
  // Creates and adds the status_area.
  void InitStatusArea();
  // Creates and adds the labels for version and boot time.
  void InitInfoLabels();
  // Creates and add OOBE progress bar.
  void InitProgressBar();
  // Creates and add GoIncoginito button.
  void InitGoIncognitoButton();

  // Updates string from the resources.
  void UpdateLocalizedStrings();

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
  views::TextButton* go_incognito_button_;

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

  // NOTE: |delegate_| is assigned to NULL when the owner is changed. See
  // 'OnOwnerChanged()' for more info.
  Delegate *delegate_;

  // DOMView for rendering a webpage as a background.
  DOMView* background_area_;

  DISALLOW_COPY_AND_ASSIGN(BackgroundView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_BACKGROUND_VIEW_H_
