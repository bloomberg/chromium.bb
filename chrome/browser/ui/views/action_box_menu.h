// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_ACTION_BOX_MENU_H_
#define CHROME_BROWSER_UI_VIEWS_ACTION_BOX_MENU_H_

#include <map>

#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "chrome/browser/ui/views/browser_action_view.h"
#include "content/public/browser/notification_observer.h"
#include "ui/views/controls/menu/menu_delegate.h"

class ActionBoxMenuModel;

namespace views {
class Background;
class Border;
class MenuItemView;
class MenuRunner;
class View;
}

// ActionBoxMenu adapts the ActionBoxMenuModel to view's menu related classes.
class ActionBoxMenu : public views::MenuDelegate,
                      public BrowserActionView::Delegate,
                      public content::NotificationObserver {
 public:
  ActionBoxMenu(Browser* browser,
                ActionBoxMenuModel* model,
                bool starred);
  virtual ~ActionBoxMenu();

  void Init();

  // Shows the menu relative to the specified button.
  void RunMenu(views::MenuButton* menu_button);

 private:
  // Overridden from views::MenuDelegate:
  virtual void ExecuteCommand(int id) OVERRIDE;
  virtual views::Border* CreateMenuBorder() OVERRIDE;
  virtual views::Background* CreateMenuBackground() OVERRIDE;

  // Overridden from BrowserActionView::Delegate and DragController overrides:
  virtual int GetCurrentTabId() const OVERRIDE;
  virtual void OnBrowserActionExecuted(BrowserActionButton* button) OVERRIDE;
  virtual void OnBrowserActionVisibilityChanged() OVERRIDE;
  virtual gfx::Point GetViewContentOffset() const OVERRIDE;
  virtual bool NeedToShowMultipleIconStates() const OVERRIDE;
  virtual bool NeedToShowTooltip() const OVERRIDE;
  virtual void WriteDragDataForView(views::View* sender,
                                    const gfx::Point& press_pt,
                                    ui::OSExchangeData* data) OVERRIDE;
  virtual int GetDragOperationsForView(views::View* sender,
                                       const gfx::Point& p) OVERRIDE;
  virtual bool CanStartDragForView(views::View* sender,
                                   const gfx::Point& press_pt,
                                   const gfx::Point& p) OVERRIDE;

  // NotificationObserver overrides:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Adds a new bookmark menu item to the menu, |next_id| is incremented.
  views::MenuItemView* AddBookmarkMenuItem(views::MenuItemView* parent,
                                           int* next_id);

  // Populates |root_| with all the child menu items from the |model_|.
  void PopulateMenu();

  Browser* browser_;

  // The views menu. Owned by |menu_runner_|.
  views::MenuItemView* root_;

  scoped_ptr<views::MenuRunner> menu_runner_;

  // The model that tracks the order of the toolbar icons.
  ActionBoxMenuModel* model_;

  // Set when the current page is bookmarked and thus the star icon should be
  // drawn in the "starred" rather than "unstarred" state.
  bool starred_;

  ScopedVector<BrowserActionView> browser_action_views_;

  DISALLOW_COPY_AND_ASSIGN(ActionBoxMenu);
};

#endif  // CHROME_BROWSER_UI_VIEWS_ACTION_BOX_MENU_H_
