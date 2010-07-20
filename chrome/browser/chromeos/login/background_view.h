// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOGIN_BACKGROUND_VIEW_H_
#define CHROME_BROWSER_CHROMEOS_LOGIN_BACKGROUND_VIEW_H_

#include "chrome/browser/chromeos/boot_times_loader.h"
#include "chrome/browser/chromeos/cros/cros_library.h"
#include "chrome/browser/chromeos/version_loader.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "views/view.h"

namespace views {
class Widget;
class Label;
}

class Profile;

namespace chromeos {

class StatusAreaView;

// View used to render the background during login. BackgroundView contains
// StatusAreaView.
class BackgroundView : public views::View, public StatusAreaHost {
 public:
  BackgroundView();

  // Creates a window containing an instance of WizardContentsView as the root
  // view. The caller is responsible for showing (and closing) the returned
  // widget. The BackgroundView is set in |view|.
  static views::Widget* CreateWindowContainingView(const gfx::Rect& bounds,
                                                   BackgroundView** view);

  // Toggles status area visibility.
  void SetStatusAreaVisible(bool visible);

 protected:
  // Overridden from views::View:
  virtual void Paint(gfx::Canvas* canvas);
  virtual void Layout();
  virtual void ChildPreferredSizeChanged(View* child);
  virtual void LocaleChanged();

  // Overridden from StatusAreaHost:
  virtual Profile* GetProfile() const { return NULL; }
  virtual gfx::NativeWindow GetNativeWindow() const;
  virtual void ExecuteBrowserCommand(int id) const {}
  virtual bool ShouldOpenButtonOptions(
      const views::View* button_view) const;
  virtual void OpenButtonOptions(const views::View* button_view) const;
  virtual bool IsButtonVisible(const views::View* button_view) const;
  virtual bool IsBrowserMode() const;
  virtual bool IsScreenLockerMode() const;

private:
  // Creates and adds the status_area.
  void InitStatusArea();
  // Creates and adds the labels for version and boot time.
  void InitInfoLabels();

  // Invokes SetWindowType for the window. This is invoked during startup and
  // after we've painted.
  void UpdateWindowType();

  // Callback from chromeos::VersionLoader giving the version.
  void OnVersion(VersionLoader::Handle handle, std::string version);
  // Callback from chromeos::InfoLoader giving the boot times.
  void OnBootTimes(
      BootTimesLoader::Handle handle, BootTimesLoader::BootTimes boot_times);

  StatusAreaView* status_area_;
  views::Label* os_version_label_;
  views::Label* boot_times_label_;

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

  DISALLOW_COPY_AND_ASSIGN(BackgroundView);
};

}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_LOGIN_BACKGROUND_VIEW_H_
