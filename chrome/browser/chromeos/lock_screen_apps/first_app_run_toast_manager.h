// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_FIRST_APP_RUN_TOAST_MANAGER_H_
#define CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_FIRST_APP_RUN_TOAST_MANAGER_H_

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "ui/aura/window_observer.h"
#include "ui/views/widget/widget_observer.h"

class Profile;

namespace views {
class Widget;
}

namespace extensions {
class AppWindow;
}

namespace lock_screen_apps {

// Manager that can be used on a lock screen note app to show a first run
// dialog informing the user about the app that's been launched from the lock
// screen.
class FirstAppRunToastManager : public aura::WindowObserver,
                                public views::WidgetObserver {
 public:
  explicit FirstAppRunToastManager(Profile* profile);
  ~FirstAppRunToastManager() override;

  // Runs the manager for an app window launch. It determines whether the first
  // lock screen run dialog for the app associated with the app window has been
  // previously shown to the user (this information is kept in the user prefs).
  // If the dialog has not yet been shown (and confirmed by the user), the
  // manager will show the dialog once the app window becomes visible.
  void RunForAppWindow(extensions::AppWindow* app_window);

  // Resets current manager state - if a first run dialog is being shown, this
  // method will close the dialog.
  void Reset();

  // views::WidgetObserver:
  void OnWidgetDestroyed(views::Widget* widget) override;

  // aura::WindowObserver:
  void OnWindowVisibilityChanged(aura::Window* window, bool visible) override;

  views::Widget* widget() { return toast_widget_; }

 private:
  // Creates and shows the first lock screen app dialog for the app associated
  // with |app_window_|.
  void CreateAndShowToastDialog();

  // Called when the user closes the first app run dialog, and thus unblocks
  // lock screen app UI.
  // The manager will mark the first run dialog as handled for the app.
  void ToastDialogDismissed();

  Profile* const profile_;

  // If set, the app window for which the manager is being run.
  extensions::AppWindow* app_window_ = nullptr;

  // The widget associated with the first run dialog, if the dialog is shown.
  views::Widget* toast_widget_ = nullptr;

  ScopedObserver<views::Widget, views::WidgetObserver> toast_widget_observer_;
  ScopedObserver<aura::Window, aura::WindowObserver> app_window_observer_;

  base::WeakPtrFactory<FirstAppRunToastManager> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(FirstAppRunToastManager);
};

}  // namespace lock_screen_apps

#endif  // CHROME_BROWSER_CHROMEOS_LOCK_SCREEN_APPS_FIRST_APP_RUN_TOAST_MANAGER_H_
