// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_VIEWS_TABS_BROWSER_TAB_STRIP_CONTROLLER_H_
#define CHROME_BROWSER_UI_VIEWS_TABS_BROWSER_TAB_STRIP_CONTROLLER_H_

#include "base/compiler_specific.h"
#include "base/memory/scoped_ptr.h"
#include "base/prefs/pref_change_registrar.h"
#include "chrome/browser/ui/tabs/hover_tab_selector.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/views/frame/immersive_mode_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"

class Browser;
class Tab;
class TabStrip;
struct TabRendererData;

namespace content {
class WebContents;
}

namespace ui {
class ListSelectionModel;
}

// An implementation of TabStripController that sources data from the
// WebContentses in a TabStripModel.
class BrowserTabStripController : public TabStripController,
                                  public TabStripModelObserver {
 public:
  BrowserTabStripController(Browser* browser, TabStripModel* model);
  virtual ~BrowserTabStripController();

  void InitFromModel(TabStrip* tabstrip);

  TabStripModel* model() const { return model_; }

  bool IsCommandEnabledForTab(TabStripModel::ContextMenuCommand command_id,
                              Tab* tab) const;
  void ExecuteCommandForTab(TabStripModel::ContextMenuCommand command_id,
                            Tab* tab);
  bool IsTabPinned(Tab* tab) const;

  // TabStripController implementation:
  virtual const ui::ListSelectionModel& GetSelectionModel() OVERRIDE;
  virtual int GetCount() const OVERRIDE;
  virtual bool IsValidIndex(int model_index) const OVERRIDE;
  virtual bool IsActiveTab(int model_index) const OVERRIDE;
  virtual int GetActiveIndex() const OVERRIDE;
  virtual bool IsTabSelected(int model_index) const OVERRIDE;
  virtual bool IsTabPinned(int model_index) const OVERRIDE;
  virtual bool IsNewTabPage(int model_index) const OVERRIDE;
  virtual void SelectTab(int model_index) OVERRIDE;
  virtual void ExtendSelectionTo(int model_index) OVERRIDE;
  virtual void ToggleSelected(int model_index) OVERRIDE;
  virtual void AddSelectionFromAnchorTo(int model_index) OVERRIDE;
  virtual void CloseTab(int model_index, CloseTabSource source) OVERRIDE;
  virtual void ShowContextMenuForTab(Tab* tab,
                                     const gfx::Point& p,
                                     ui::MenuSourceType source_type) OVERRIDE;
  virtual void UpdateLoadingAnimations() OVERRIDE;
  virtual int HasAvailableDragActions() const OVERRIDE;
  virtual void OnDropIndexUpdate(int index, bool drop_before) OVERRIDE;
  virtual void PerformDrop(bool drop_before,
                           int index,
                           const GURL& url) OVERRIDE;
  virtual bool IsCompatibleWith(TabStrip* other) const OVERRIDE;
  virtual void CreateNewTab() OVERRIDE;
  virtual void CreateNewTabWithLocation(const base::string16& loc) OVERRIDE;
  virtual bool IsIncognito() OVERRIDE;
  virtual void StackedLayoutMaybeChanged() OVERRIDE;
  virtual void OnStartedDraggingTabs() OVERRIDE;
  virtual void OnStoppedDraggingTabs() OVERRIDE;
  virtual void CheckFileSupported(const GURL& url) OVERRIDE;

  // TabStripModelObserver implementation:
  virtual void TabInsertedAt(content::WebContents* contents,
                             int model_index,
                             bool is_active) OVERRIDE;
  virtual void TabDetachedAt(content::WebContents* contents,
                             int model_index) OVERRIDE;
  virtual void TabSelectionChanged(
      TabStripModel* tab_strip_model,
      const ui::ListSelectionModel& old_model) OVERRIDE;
  virtual void TabMoved(content::WebContents* contents,
                        int from_model_index,
                        int to_model_index) OVERRIDE;
  virtual void TabChangedAt(content::WebContents* contents,
                            int model_index,
                            TabChangeType change_type) OVERRIDE;
  virtual void TabReplacedAt(TabStripModel* tab_strip_model,
                             content::WebContents* old_contents,
                             content::WebContents* new_contents,
                             int model_index) OVERRIDE;
  virtual void TabPinnedStateChanged(content::WebContents* contents,
                                     int model_index) OVERRIDE;
  virtual void TabMiniStateChanged(content::WebContents* contents,
                                   int model_index) OVERRIDE;
  virtual void TabBlockedStateChanged(content::WebContents* contents,
                                      int model_index) OVERRIDE;

 protected:
  // The context in which SetTabRendererDataFromModel is being called.
  enum TabStatus {
    NEW_TAB,
    EXISTING_TAB
  };

  // Sets the TabRendererData from the TabStripModel.
  virtual void SetTabRendererDataFromModel(content::WebContents* contents,
                                           int model_index,
                                           TabRendererData* data,
                                           TabStatus tab_status);

  Profile* profile() const { return model_->profile(); }

  const TabStrip* tabstrip() const { return tabstrip_; }

  const Browser* browser() const { return browser_; }

 private:
  class TabContextMenuContents;

  // Invokes tabstrip_->SetTabData.
  void SetTabDataAt(content::WebContents* web_contents, int model_index);

  void StartHighlightTabsForCommand(
      TabStripModel::ContextMenuCommand command_id,
      Tab* tab);
  void StopHighlightTabsForCommand(
      TabStripModel::ContextMenuCommand command_id,
      Tab* tab);

  // Adds a tab.
  void AddTab(content::WebContents* contents, int index, bool is_active);

  // Resets the tabstrips stacked layout (true or false) from prefs.
  void UpdateStackedLayout();

  // Notifies the tabstrip whether |url| is supported once a MIME type request
  // has completed.
  void OnFindURLMimeTypeCompleted(const GURL& url,
                                  const std::string& mime_type);

  TabStripModel* model_;

  TabStrip* tabstrip_;

  // Non-owning pointer to the browser which is using this controller.
  Browser* browser_;

  // If non-NULL it means we're showing a menu for the tab.
  scoped_ptr<TabContextMenuContents> context_menu_contents_;

  // Helper for performing tab selection as a result of dragging over a tab.
  HoverTabSelector hover_tab_selector_;

  // Forces the tabs to use the regular (non-immersive) style and the
  // top-of-window views to be revealed when the user is dragging |tabstrip|'s
  // tabs.
  scoped_ptr<ImmersiveRevealedLock> immersive_reveal_lock_;

  PrefChangeRegistrar local_pref_registrar_;

  base::WeakPtrFactory<BrowserTabStripController> weak_ptr_factory_;

  DISALLOW_COPY_AND_ASSIGN(BrowserTabStripController);
};

#endif  // CHROME_BROWSER_UI_VIEWS_TABS_BROWSER_TAB_STRIP_CONTROLLER_H_
