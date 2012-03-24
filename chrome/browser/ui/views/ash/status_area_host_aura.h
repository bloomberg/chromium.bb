// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ASH_STATUS_AREA_HOST_AURA_H_
#define CHROME_BROWSER_UI_VIEWS_ASH_STATUS_AREA_HOST_AURA_H_
#pragma once

#include "base/compiler_specific.h"
#include "chrome/browser/chromeos/status/status_area_button.h"
#include "chrome/browser/ui/browser_list.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"

#if defined(OS_CHROMEOS)
#include "base/memory/scoped_ptr.h"
#include "chrome/browser/chromeos/login/login_html_dialog.h"
#endif

class ClockUpdater;
class StatusAreaView;

namespace views {
class Views;
class Widget;
}

class StatusAreaHostAura : public StatusAreaButton::Delegate,
                           public BrowserList::Observer,
                           public content::NotificationObserver {
 public:
  StatusAreaHostAura();
  virtual ~StatusAreaHostAura();

  // Returns the status area view.
  StatusAreaView* GetStatusArea();

  // Instantiates and sets |status_area_view_|, and sets it as the contents of
  // a new views::Widget |status_area_widget_| which is returned.
  // The caller is expected to take ownership of |status_area_widget_|.
  views::Widget* CreateStatusArea();

  // Adds the buttons to the status area. This is called separately, after
  // the profile has been initialized.
  void AddButtons();

  // StatusAreaButton::Delegate implementation.
  virtual bool ShouldExecuteStatusAreaCommand(
      const views::View* button_view, int command_id) const OVERRIDE;
  virtual void ExecuteStatusAreaCommand(
      const views::View* button_view, int command_id) OVERRIDE;
  virtual StatusAreaButton::TextStyle GetStatusAreaTextStyle() const OVERRIDE;
  virtual void ButtonVisibilityChanged(views::View* button_view) OVERRIDE;

  // BrowserList::Observer implementation.
  virtual void OnBrowserAdded(const Browser* browser) OVERRIDE {}
  virtual void OnBrowserRemoved(const Browser* browser) OVERRIDE {}
  virtual void OnBrowserSetLastActive(const Browser* browser) OVERRIDE;

  // content::NotificationObserver implementation.
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

 private:
  // Is either the login or lock screen currently displayed?
  bool IsLoginOrLockScreenDisplayed() const;

  // Triggers an update of the status area text style and position.
  void UpdateAppearance();

  // Owned by caller of CreateStatusArea().
  views::Widget* status_area_widget_;
  // Owned by status_area_widget_.
  StatusAreaView* status_area_view_;

#if defined(OS_CHROMEOS)
  scoped_ptr<ClockUpdater> clock_updater_;
#endif

  content::NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(StatusAreaHostAura);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ASH_STATUS_AREA_HOST_AURA_H_
