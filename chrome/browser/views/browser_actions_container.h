// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BROWSER_ACTIONS_PANEL_H_
#define CHROME_BROWSER_VIEWS_BROWSER_ACTIONS_PANEL_H_

#include <vector>

#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "views/view.h"

class ExtensionAction;
class Profile;
class ToolbarView;
namespace views {
class ImageButton;
}

////////////////////////////////////////////////////////////////////////////////
//
// The BrowserActionsContainer is a container view, responsible for drawing the
// icons that represent browser actions (extensions that add icons to the
// toolbar).
//
////////////////////////////////////////////////////////////////////////////////
class BrowserActionsContainer : public views::View,
                                public NotificationObserver {
 public:
  BrowserActionsContainer(Profile* profile, ToolbarView* toolbar);
  virtual ~BrowserActionsContainer();

  // Update the views to reflect the state of the browser action icons.
  void RefreshBrowserActionViews();

  // Delete all browser action views.
  void DeleteBrowserActionViews();

  // Called when a browser action becomes visible/hidden.
  void OnBrowserActionVisibilityChanged();

  // Called when the user clicks on the browser action icon.
  void OnBrowserActionExecuted(const ExtensionAction& browser_action);

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

 private:
  // The vector of browser actions (icons/image buttons for each action).
  std::vector<views::ImageButton*> browser_action_views_;

  NotificationRegistrar registrar_;

  Profile* profile_;

  // The toolbar that owns us.
  ToolbarView* toolbar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionsContainer);
};

#endif  // CHROME_BROWSER_VIEWS_BROWSER_ACTIONS_PANEL_H_
