// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_EXPERIMENTAL_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_EXPERIMENTAL_H_

#include <memory>

#include "base/containers/span.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/tabs/tab_data_experimental.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/page_transition_types.h"

class TabDataExperimental;
class TabStripModelExperimentalObserver;
class TabStripModelExperimentalTest;

class TabStripModelExperimental : public TabStripModel {
 public:
  class ConstViewIterator {
   public:
    friend TabStripModelExperimental;

    using difference_type = ptrdiff_t;
    using value_type = TabDataExperimental;
    using pointer = TabDataExperimental*;
    using reference = TabDataExperimental&;
    using iterator_category = std::bidirectional_iterator_tag;

    ConstViewIterator();

    const TabDataExperimental& operator*() const;
    const TabDataExperimental* operator->() const;

    ConstViewIterator& operator++();
    ConstViewIterator operator++(int);
    ConstViewIterator& operator--();
    ConstViewIterator operator--(int);

    bool operator==(const ConstViewIterator& other) const;
    bool operator!=(const ConstViewIterator& other) const;
    bool operator<(const ConstViewIterator& other) const;
    bool operator>(const ConstViewIterator& other) const;
    bool operator<=(const ConstViewIterator& other) const;
    bool operator>=(const ConstViewIterator& other) const;

   protected:
    ConstViewIterator(const TabStripModelExperimental* model,
                      int toplevel_index,
                      int inner_index);

    void Increment();
    void Decrement();

    const TabStripModelExperimental* model_ = nullptr;

    // Index into model_.tabs_.
    int toplevel_index_ = 0;

    // Index into model_.tabs_[toplevel_index].children_. -1 indicates this
    // iterator refers to the toplevel data and not into its children_.
    int inner_index_ = 0;
  };

  class ViewIterator : public ConstViewIterator {
   public:
    friend TabStripModelExperimental;

    ViewIterator();

    TabDataExperimental& operator*() const;
    TabDataExperimental* operator->() const;

    ViewIterator& operator++();
    ViewIterator operator++(int);
    ViewIterator& operator--();
    ViewIterator operator--(int);

   private:
    ViewIterator(TabStripModelExperimental* model,
                 int toplevel_index,
                 int inner_index);
  };

  TabStripModelExperimental(TabStripModelDelegate* delegate, Profile* profile);
  ~TabStripModelExperimental() override;

  ConstViewIterator begin() const { return ConstViewIterator(this, 0, -1); }
  ViewIterator begin() { return ViewIterator(this, 0, -1); }
  ConstViewIterator end() const {
    return ConstViewIterator(this, static_cast<int>(tabs_.size()), -1);
  }
  ViewIterator end() {
    return ViewIterator(this, static_cast<int>(tabs_.size()), -1);
  }

  // TabStripModel implementation.
  TabStripModelExperimental* AsTabStripModelExperimental() override;
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
  bool CloseWebContentsAt(int view_index, uint32_t close_types) override;
  content::WebContents* ReplaceWebContentsAt(
      int view_index,
      content::WebContents* new_contents) override;
  content::WebContents* DetachWebContentsAt(int index) override;
  void ActivateTabAt(int index, bool user_gesture) override;
  void AddTabAtToSelection(int index) override;
  void MoveWebContentsAt(int index,
                         int to_position,
                         bool select_after_move) override;
  void MoveSelectedTabsTo(int index) override;
  content::WebContents* GetActiveWebContents() const override;
  content::WebContents* GetWebContentsAt(int view_index) const override;
  int GetIndexOfWebContents(
      const content::WebContents* contents) const override;
  void UpdateWebContentsStateAt(
      int view_index,
      TabStripModelObserver::TabChangeType change_type) override;
  void SetTabNeedsAttentionAt(int index, bool attention) override;
  void CloseAllTabs() override;
  bool TabsAreLoading() const override;
  content::WebContents* GetOpenerOfWebContentsAt(int index) override;
  void SetOpenerOfWebContentsAt(int index,
                                content::WebContents* opener) override;
  int GetIndexOfLastWebContentsOpenedBy(const content::WebContents* opener,
                                        int start_index) const override;
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

  // Returns the tab model data.
  const std::vector<std::unique_ptr<TabDataExperimental>>& top_level_tabs()
      const {
    return tabs_;
  }

  // Prefer over DetachWebContentsAt since this doesn't round-trip through view
  // indices.
  void DetachWebContents(content::WebContents* web_contents);

  // Returns the data associated with the given WebContents, or null if it
  // is not in the model.
  TabDataExperimental* GetDataForWebContents(
      content::WebContents* web_contents);

  // Returns the data for the given view index if it exists, or null of the
  // index is not valid.
  const TabDataExperimental* GetDataForViewIndex(int view_index) const;
  TabDataExperimental* GetDataForViewIndex(int view_index);

  void AddExperimentalObserver(TabStripModelExperimentalObserver* observer);
  void RemoveExperimentalObserver(TabStripModelExperimentalObserver* observer);

 private:
  friend TabStripModelExperimentalTest;
  friend ViewIterator;

  // Used when making selection notifications.
  enum class Notify {
    kDefault,

    // The selection is changing from a user gesture.
    kUserGesture,
  };

  // Sets the selection to |new_model| and notifies any observers.
  // Note: This function might end up sending 0 to 3 notifications in the
  // following order: TabDeactivated, ActiveTabChanged, TabSelectionChanged.
  void SetSelection(ui::ListSelectionModel new_model, Notify notify_types);

  // Notifies the observers if the active tab is being deactivated.
  void NotifyIfTabDeactivated(content::WebContents* contents);

  // Notifies the observers if the active tab has changed.
  void NotifyIfActiveTabChanged(content::WebContents* old_contents,
                                Notify notify_types);

  // Notifies the observers if the active tab or the tab selection has changed.
  // |old_model| is a snapshot of |selection_model_| before the change.
  // Note: This function might end up sending 0 to 2 notifications in the
  // following order: ActiveTabChanged, TabSelectionChanged.
  void NotifyIfActiveOrSelectionChanged(
      content::WebContents* old_contents,
      Notify notify_types,
      const ui::ListSelectionModel& old_model);

  void InternalCloseTabs(base::span<content::WebContents*> tabs_to_close);

  // Returns the iterator associated with the give view index, or end() if
  // not found.
  ConstViewIterator FindViewIndex(int view_index) const;
  ViewIterator FindViewIndex(int view_index);
  int ComputeViewCount() const;
  void UpdateViewCount();

  TabStripModelDelegate* delegate_;
  Profile* profile_;

  base::ObserverList<TabStripModelObserver> observers_;
  base::ObserverList<TabStripModelExperimentalObserver> exp_observers_;

  ui::ListSelectionModel selection_model_;
  bool closing_all_ = false;

  // Flag to check for reentrant notifications.
  bool in_notify_ = false;

  std::vector<std::unique_ptr<TabDataExperimental>> tabs_;

  // Number of "view" tabs in tabs_. This is updated by UpdateViewCount().
  int tab_view_count_ = 0;

  base::WeakPtrFactory<TabStripModelExperimental> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(TabStripModelExperimental);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_EXPERIMENTAL_H_
