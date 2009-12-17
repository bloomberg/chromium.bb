// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_VIEWS_BROWSER_ACTIONS_CONTAINER_H_
#define CHROME_BROWSER_VIEWS_BROWSER_ACTIONS_CONTAINER_H_

#include <vector>

#include "base/task.h"
#include "chrome/browser/extensions/image_loading_tracker.h"
#include "chrome/browser/views/browser_bubble.h"
#include "chrome/browser/views/extensions/extension_action_context_menu.h"
#include "chrome/common/notification_observer.h"
#include "chrome/common/notification_registrar.h"
#include "views/controls/button/menu_button.h"
#include "views/view.h"

class BrowserActionsContainer;
class Extension;
class ExtensionAction;
class ExtensionPopup;
class Profile;
class ToolbarView;

////////////////////////////////////////////////////////////////////////////////
// BrowserActionButton

// The BrowserActionButton is a specialization of the MenuButton class.
// It acts on a ExtensionAction, in this case a BrowserAction and handles
// loading the image for the button asynchronously on the file thread to
class BrowserActionButton : public views::MenuButton,
                            public views::ButtonListener,
                            public ImageLoadingTracker::Observer,
                            public NotificationObserver {
 public:
  BrowserActionButton(Extension* extension, BrowserActionsContainer* panel);
  ~BrowserActionButton();

  ExtensionAction* browser_action() const { return browser_action_; }
  Extension* extension() { return extension_; }

  // Called to update the display to match the browser action's state.
  void UpdateState();

  // Overridden from views::View. Return a 0-inset so the icon can draw all the
  // way to the edge of the view if it wants.
  virtual gfx::Insets GetInsets() const;

  // Overridden from views::ButtonListener:
  virtual void ButtonPressed(views::Button* sender, const views::Event& event);

  // Overridden from ImageLoadingTracker.
  virtual void OnImageLoaded(SkBitmap* image, size_t index);

  // Overridden from NotificationObserver:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details);

  // MenuButton behavior overrides.  These methods all default to TextButton
  // behavior unless this button is a popup.  In that case, it uses MenuButton
  // behavior.  MenuButton has the notion of a child popup being shown where the
  // button will stay in the pushed state until the "menu" (a popup in this
  // case) is dismissed.
  virtual bool Activate();
  virtual bool OnMousePressed(const views::MouseEvent& e);
  virtual void OnMouseReleased(const views::MouseEvent& e, bool canceled);
  virtual bool OnKeyReleased(const views::KeyEvent& e);
  virtual void OnMouseExited(const views::MouseEvent& event);

  // Does this button's action have a popup?
  virtual bool IsPopup();

  // Notifications when to set button state to pushed/not pushed (for when the
  // popup/context menu is hidden or shown by the container).
  virtual void SetButtonPushed();
  virtual void SetButtonNotPushed();

 private:
  // The browser action this view represents. The ExtensionAction is not owned
  // by this class.
  ExtensionAction* browser_action_;

  // The extension associated with the browser action we're displaying.
  Extension* extension_;

  // The object that is waiting for the image loading to complete
  // asynchronously. This object can potentially outlive the BrowserActionView,
  // and takes care of deleting itself.
  ImageLoadingTracker* tracker_;

  // The context menu for browser action icons.
  scoped_ptr<ExtensionActionContextMenu> context_menu_;

  // Whether we are currently showing/just finished showing a context menu.
  bool showing_context_menu_;

  // The default icon for our browser action. This might be non-empty if the
  // browser action had a value for default_icon in the manifest.
  SkBitmap default_icon_;

  // The browser action shelf.
  BrowserActionsContainer* panel_;

  NotificationRegistrar registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserActionButton);
};


////////////////////////////////////////////////////////////////////////////////
// BrowserActionView
// A single section in the browser action container. This contains the actual
// BrowserActionButton, as well as the logic to paint the badge.

class BrowserActionView : public views::View {
 public:
  BrowserActionView(Extension* extension, BrowserActionsContainer* panel);
  BrowserActionButton* button() { return button_; }

 private:
  virtual void Layout();

  // Override PaintChildren so that we can paint the badge on top of children.
  virtual void PaintChildren(gfx::Canvas* canvas);

  // The container for this view.
  BrowserActionsContainer* panel_;

  // The button this view contains.
  BrowserActionButton* button_;
};


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

  // Get the number of browser actions being displayed.
  int num_browser_actions() { return browser_action_views_.size(); }

  // Returns the current tab's ID, or -1 if there is no current tab.
  int GetCurrentTabId();

  // Get a particular browser action view.
  BrowserActionView* GetBrowserActionViewAt(int index) {
    return browser_action_views_[index];
  }

  // Retrieve the BrowserActionView for |extension|.
  BrowserActionView* GetBrowserActionView(Extension* extension);

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
  virtual void BubbleLostFocus(BrowserBubble* bubble,
                               gfx::NativeView focused_view);

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
  // Adds a browser action view for the extension if it needs one. DCHECK if
  // it has already been added.
  void AddBrowserAction(Extension* extension);

  // Removes the browser action view for an extension if it has one. DCHECK if
  // no such view.
  void RemoveBrowserAction(Extension* extension);

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

#endif  // CHROME_BROWSER_VIEWS_BROWSER_ACTIONS_CONTAINER_H_
