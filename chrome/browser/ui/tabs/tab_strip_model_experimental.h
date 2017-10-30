// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_EXPERIMENTAL_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_EXPERIMENTAL_H_

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/page_transition_types.h"

class TabStripModelExperimental : public TabStripModel {
 public:
  TabStripModelExperimental(TabStripModelDelegate* delegate, Profile* profile);
  ~TabStripModelExperimental() override;

  // TabStripModel implementation.
  TabStripModelDelegate* delegate() const override;
  void AddObserver(TabStripModelObserver* observer) override;
  void RemoveObserver(TabStripModelObserver* observer) override;
  int count() const override;
  bool empty() const override;
  Profile* profile() const override;
  int active_index() const override;
  bool closing_all() const override;
  bool ContainsIndex(int index) const override;
  void AppendWebContents(content::WebContents* contents,
                         bool foreground) override;
  void InsertWebContentsAt(int index,
                           content::WebContents* contents,
                           int add_types) override;
  bool CloseWebContentsAt(int index, uint32_t close_types) override;
  content::WebContents* ReplaceWebContentsAt(
      int index,
      content::WebContents* new_contents) override;
  content::WebContents* DetachWebContentsAt(int index) override;
  void ActivateTabAt(int index, bool user_gesture) override;
  void AddTabAtToSelection(int index) override;
  void MoveWebContentsAt(int index,
                         int to_position,
                         bool select_after_move) override;
  void MoveSelectedTabsTo(int index) override;
  content::WebContents* GetActiveWebContents() const override;
  content::WebContents* GetWebContentsAt(int index) const override;
  int GetIndexOfWebContents(
      const content::WebContents* contents) const override;
  void UpdateWebContentsStateAt(
      int index,
      TabStripModelObserver::TabChangeType change_type) override;
  void SetTabNeedsAttentionAt(int index, bool attention) override;
  void CloseAllTabs() override;
  bool TabsAreLoading() const override;
  content::WebContents* GetOpenerOfWebContentsAt(int index) override;
  void SetOpenerOfWebContentsAt(int index,
                                content::WebContents* opener) override;
  void TabNavigating(content::WebContents* contents,
                     ui::PageTransition transition) override;
  void SetTabBlocked(int index, bool blocked) override;
  void SetTabPinned(int index, bool pinned) override;
  bool IsTabPinned(int index) const override;
  bool IsTabBlocked(int index) const override;
  int IndexOfFirstNonPinnedTab() const override;
  void ExtendSelectionTo(int index) override;
  void ToggleSelectionAt(int index) override;
  void AddSelectionFromAnchorTo(int index) override;
  bool IsTabSelected(int index) const override;
  void SetSelectionFromModel(ui::ListSelectionModel source) override;
  const ui::ListSelectionModel& selection_model() const override;
  void AddWebContents(content::WebContents* contents,
                      int index,
                      ui::PageTransition transition,
                      int add_types) override;
  void CloseSelectedTabs() override;
  void SelectNextTab() override;
  void SelectPreviousTab() override;
  void SelectLastTab() override;
  void MoveTabNext() override;
  void MoveTabPrevious() override;
  bool IsContextMenuCommandEnabled(
      int context_index,
      ContextMenuCommand command_id) const override;
  void ExecuteContextMenuCommand(int context_index,
                                 ContextMenuCommand command_id) override;
  std::vector<int> GetIndicesClosedByCommand(int index, ContextMenuCommand id)
      const override;
  bool WillContextMenuMute(int index) override;
  bool WillContextMenuMuteSites(int index) override;
  bool WillContextMenuPin(int index) override;

 private:
  TabStripModelDelegate* delegate_;
  Profile* profile_;

  base::ObserverList<TabStripModelObserver> observers_;

  ui::ListSelectionModel selection_model_;

  DISALLOW_COPY_AND_ASSIGN(TabStripModelExperimental);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_EXPERIMENTAL_H_
