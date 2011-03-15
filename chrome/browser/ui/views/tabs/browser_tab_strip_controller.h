// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_BROWSER_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_BROWSER_TAB_STRIP_CONTROLLER_H_
#pragma once

#include "base/scoped_ptr.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "content/common/notification_observer.h"
#include "content/common/notification_registrar.h"

class BaseTab;
class BaseTabStrip;
class Browser;

struct TabRendererData;

// An implementation of TabStripController that sources data from the
// TabContentsWrappers in a TabStripModel.
class BrowserTabStripController : public TabStripController,
                                  public TabStripModelObserver,
                                  public NotificationObserver {
 public:
  BrowserTabStripController(Browser* browser, TabStripModel* model);
  virtual ~BrowserTabStripController();

  void InitFromModel(BaseTabStrip* tabstrip);

  TabStripModel* model() const { return model_; }

  bool IsCommandEnabledForTab(TabStripModel::ContextMenuCommand command_id,
                              BaseTab* tab) const;
  bool IsCommandCheckedForTab(TabStripModel::ContextMenuCommand command_id,
                              BaseTab* tab) const;
  void ExecuteCommandForTab(TabStripModel::ContextMenuCommand command_id,
                            BaseTab* tab);
  bool IsTabPinned(BaseTab* tab) const;

  // TabStripController implementation:
  virtual int GetCount() const OVERRIDE;
  virtual bool IsValidIndex(int model_index) const OVERRIDE;
  virtual bool IsTabSelected(int model_index) const OVERRIDE;
  virtual bool IsTabPinned(int model_index) const OVERRIDE;
  virtual bool IsTabCloseable(int model_index) const OVERRIDE;
  virtual bool IsNewTabPage(int model_index) const OVERRIDE;
  virtual void SelectTab(int model_index) OVERRIDE;
  virtual void ExtendSelectionTo(int model_index) OVERRIDE;
  virtual void ToggleSelected(int model_index) OVERRIDE;
  virtual void CloseTab(int model_index) OVERRIDE;
  virtual void ShowContextMenuForTab(BaseTab* tab,
                                     const gfx::Point& p) OVERRIDE;
  virtual void UpdateLoadingAnimations() OVERRIDE;
  virtual int HasAvailableDragActions() const OVERRIDE;
  virtual void PerformDrop(bool drop_before,
                           int index,
                           const GURL& url) OVERRIDE;
  virtual bool IsCompatibleWith(BaseTabStrip* other) const OVERRIDE;
  virtual void CreateNewTab() OVERRIDE;

  // TabStripModelObserver implementation:
  virtual void TabInsertedAt(TabContentsWrapper* contents,
                             int model_index,
                             bool active) OVERRIDE;
  virtual void TabDetachedAt(TabContentsWrapper* contents,
                             int model_index) OVERRIDE;
  virtual void TabSelectedAt(TabContentsWrapper* old_contents,
                             TabContentsWrapper* contents,
                             int model_index,
                             bool user_gesture) OVERRIDE;
  virtual void TabMoved(TabContentsWrapper* contents,
                        int from_model_index,
                        int to_model_index) OVERRIDE;
  virtual void TabChangedAt(TabContentsWrapper* contents,
                            int model_index,
                            TabChangeType change_type) OVERRIDE;
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents,
                             int model_index) OVERRIDE;
  virtual void TabPinnedStateChanged(TabContentsWrapper* contents,
                                     int model_index) OVERRIDE;
  virtual void TabMiniStateChanged(TabContentsWrapper* contents,
                                   int model_index) OVERRIDE;
  virtual void TabBlockedStateChanged(TabContentsWrapper* contents,
                                      int model_index) OVERRIDE;

  // NotificationObserver implementation:
  virtual void Observe(NotificationType type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 private:
  class TabContextMenuContents;

  // Invokes tabstrip_->SetTabData.
  void SetTabDataAt(TabContentsWrapper* contents, int model_index);

  // Sets the TabRendererData from the TabStripModel.
  void SetTabRendererDataFromModel(TabContents* contents,
                                   int model_index,
                                   TabRendererData* data);

  void StartHighlightTabsForCommand(
      TabStripModel::ContextMenuCommand command_id,
      BaseTab* tab);
  void StopHighlightTabsForCommand(
      TabStripModel::ContextMenuCommand command_id,
      BaseTab* tab);

  Profile* profile() const { return model_->profile(); }

  TabStripModel* model_;

  BaseTabStrip* tabstrip_;

  // Non-owning pointer to the browser which is using this controller.
  Browser* browser_;

  // If non-NULL it means we're showing a menu for the tab.
  scoped_ptr<TabContextMenuContents> context_menu_contents_;

  NotificationRegistrar notification_registrar_;

  DISALLOW_COPY_AND_ASSIGN(BrowserTabStripController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_BROWSER_TAB_STRIP_CONTROLLER_H_

