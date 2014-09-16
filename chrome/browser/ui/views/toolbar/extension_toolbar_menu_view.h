// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TOOLBAR_EXTENSION_TOOLBAR_MENU_VIEW_H_
#define CHROME_BROWSER_UI_VIEWS_TOOLBAR_EXTENSION_TOOLBAR_MENU_VIEW_H_

#include "base/memory/weak_ptr.h"
#include "base/scoped_observer.h"
#include "chrome/browser/ui/views/toolbar/browser_actions_container_observer.h"
#include "ui/views/view.h"

class Browser;
class BrowserActionsContainer;
class WrenchMenu;

// ExtensionToolbarMenuView is the view containing the extension actions that
// overflowed from the BrowserActionsContainer, and is contained in and owned by
// the wrench menu.
// In the event that the WrenchMenu was opened for an Extension Action drag-and-
// drop, this will also close the menu upon completion.
class ExtensionToolbarMenuView : public views::View,
                                 public BrowserActionsContainerObserver {
 public:
  ExtensionToolbarMenuView(Browser* browser, WrenchMenu* wrench_menu);
  virtual ~ExtensionToolbarMenuView();

  // Returns whether the wrench menu should show this view. This is true when
  // either |container_| has icons to display or the menu was opened for a drag-
  // and-drop operation.
  bool ShouldShow();

  // views::View:
  virtual gfx::Size GetPreferredSize() const OVERRIDE;
  virtual int GetHeightForWidth(int width) const OVERRIDE;
  virtual void Layout() OVERRIDE;

 private:
  // BrowserActionsContainerObserver:
  virtual void OnBrowserActionDragDone() OVERRIDE;

  // Closes the |wrench_menu_|.
  void CloseWrenchMenu();

  // The associated browser.
  Browser* browser_;

  // The WrenchMenu, which may need to be closed after a drag-and-drop.
  WrenchMenu* wrench_menu_;

  // The overflow BrowserActionsContainer which is nested in this view.
  BrowserActionsContainer* container_;

  ScopedObserver<BrowserActionsContainer, BrowserActionsContainerObserver>
      browser_actions_container_observer_;

  base::WeakPtrFactory<ExtensionToolbarMenuView> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(ExtensionToolbarMenuView);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TOOLBAR_EXTENSION_TOOLBAR_MENU_VIEW_H_
