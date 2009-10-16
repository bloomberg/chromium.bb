// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BROWSER_ACTIONS_PANEL_H_
#define CHROME_BROWSER_VIEWS_BROWSER_ACTIONS_PANEL_H_

#include <vector>

#include "base/task.h"
#include "chrome/browser/views/browser_bubble.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "views/view.h"

class BrowserActionButton;
class BrowserActionView;
class ExtensionAction;
class ExtensionPopup;
class Profile;
class ToolbarView;

////////////////////////////////////////////////////////////////////////////////
//
// The BrowserActionsContainer is a container view, responsible for drawing the
// icons that represent browser actions (extensions that add icons to the
// toolbar).
//
////////////////////////////////////////////////////////////////////////////////
class BrowserActionsContainer : public views::View,
                                public NotificationObserver,
                                public BrowserBubble::Delegate {
 public:
  BrowserActionsContainer(Profile* profile, ToolbarView* toolbar);
  virtual ~BrowserActionsContainer();

  int num_browser_actions() { return browser_action_views_.size(); }

  // Update the views to reflect the state of the browser action icons.
  void RefreshBrowserActionViews();

  // Delete all browser action views.
  void DeleteBrowserActionViews();

  // Called when a browser action becomes visible/hidden.
  void OnBrowserActionVisibilityChanged();

  // Called when the user clicks on the browser action icon.
  void OnBrowserActionExecuted(BrowserActionButton* button);

  // Overridden from views::View:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // BrowserBubble::Delegate methods.
  virtual void BubbleBrowserWindowMoved(BrowserBubble* bubble);
  virtual void BubbleBrowserWindowClosing(BrowserBubble* bubble);
  virtual void BubbleGotFocus(BrowserBubble* bubble);
  virtual void BubbleLostFocus(BrowserBubble* bubble);

  // Get clipped width required to precisely fit the browser action icons
  // given a tentative available width. The minimum size it returns is not
  // zero, but depends on the minimum number of icons that have to be there
  // by default irrespective of the available space to draw them.
  int GetClippedPreferredWidth(int available_width);

  // Hide the current popup.
  void HidePopup();

  // Simulate a click on a browser action button.  This should only be
  // used by unit tests.
  void TestExecuteBrowserAction(int index);

  // Retrieve the current popup.  This should only be used by unit tests.
  ExtensionPopup* TestGetPopup() { return popup_; }

 private:
  // The vector of browser actions (icons/image buttons for each action).
  std::vector<BrowserActionView*> browser_action_views_;

  NotificationRegistrar registrar_;

  Profile* profile_;

  // The toolbar that owns us.
  ToolbarView* toolbar_;

  // The current popup and the button it came from.  NULL if no popup.
  ExtensionPopup* popup_;

  // The button that triggered the current popup (just a reference to a button
  // from browser_action_views_).
  BrowserActionButton* popup_button_;

  ScopedRunnableMethodFactory<BrowserActionsContainer> task_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionsContainer);
};

#endif  // CHROME_BROWSER_VIEWS_BROWSER_ACTIONS_PANEL_H_
