// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_IMPL_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_IMPL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/page_transition_types.h"

class Profile;
class TabStripModelDelegate;
class TabStripModelOrderController;

namespace content {
class WebContents;
}

// Normal implementation of the TabStripModel interface.
class TabStripModelImpl : public TabStripModel {
 public:
  // Construct a TabStripModel with a delegate to help it do certain things
  // (see the TabStripModelDelegate documentation). |delegate| cannot be NULL.
  TabStripModelImpl(TabStripModelDelegate* delegate, Profile* profile);
  ~TabStripModelImpl() override;

  // TabStripModel implementation.
  TabStripModelExperimental* AsTabStripModelExperimental() override;
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

  //////////////////////////////////////////////////////////////////////////////
  // Ordering API
  //
  // These functions are used only by the TabStripModelOrderController which
  // goes with this class (not the abstract base-class) so these do not need
  // to be exposed in the general TabStripModel interface.

  // Access the order controller. Exposed only for unit tests.
  TabStripModelOrderController* order_controller() const {
    return order_controller_.get();
  }

  // Returns the index of the next WebContents in the sequence of WebContentses
  // spawned by the specified WebContents after |start_index|. If |use_group| is
  // true, the group property of the tab is used instead of the opener to find
  // the next tab. Under some circumstances the group relationship may exist but
  // the opener may not.
  int GetIndexOfNextWebContentsOpenedBy(const content::WebContents* opener,
                                        int start_index,
                                        bool use_group) const;

  // Forget all Opener relationships that are stored (but _not_ group
  // relationships!) This is to reduce unpredictable tab switching behavior
  // in complex session states. The exact circumstances under which this method
  // is called are left up to the implementation of the selected
  // TabStripModelOrderController.
  void ForgetAllOpeners();

  // Forgets the group affiliation of the specified WebContents. This
  // should be called when a WebContents that is part of a logical group
  // of tabs is moved to a new logical context by the user (e.g. by typing a new
  // URL or selecting a bookmark). This also forgets the opener, which is
  // considered a weaker relationship than group.
  void ForgetGroup(content::WebContents* contents);

  // Returns true if the group/opener relationships present for |contents|
  // should be reset when _any_ selection change occurs in the model.
  bool ShouldResetGroupOnSelect(content::WebContents* contents) const;

 private:
  class WebContentsData;

  // Used when making selection notifications.
  enum class Notify {
    kDefault,

    // The selection is changing from a user gesture.
    kUserGesture,
  };

  int ConstrainInsertionIndex(int index, bool pinned_tab);

  // Convenience for converting a vector of indices into a vector of
  // WebContents.
  std::vector<content::WebContents*> GetWebContentsFromIndices(
      const std::vector<int>& indices) const;

  // Gets the set of tab indices whose domain matches the tab at |index|.
  void GetIndicesWithSameDomain(int index, std::vector<int>* indices);

  // Gets the set of tab indices that have the same opener as the tab at
  // |index|.
  void GetIndicesWithSameOpener(int index, std::vector<int>* indices);

  // If |index| is selected all the selected indices are returned, otherwise a
  // vector with |index| is returned. This is used when executing commands to
  // determine which indices the command applies to.
  std::vector<int> GetIndicesForCommand(int index) const;

  // Returns true if the specified WebContents is a New Tab at the end of
  // the tabstrip. We check for this because opener relationships are _not_
  // forgotten for the New Tab page opened as a result of a New Tab gesture
  // (e.g. Ctrl+T, etc) since the user may open a tab transiently to look up
  // something related to their current activity.
  bool IsNewTabAtEndOfTabStrip(content::WebContents* contents) const;

  // Closes the WebContentses at the specified indices. This causes the
  // WebContentses to be destroyed, but it may not happen immediately. If
  // the page in question has an unload event the WebContents will not be
  // destroyed until after the event has completed, which will then call back
  // into this method.
  //
  // Returns true if the WebContentses were closed immediately, false if we
  // are waiting for the result of an onunload handler.
  bool InternalCloseTabs(const std::vector<content::WebContents*>& items,
                         uint32_t close_types);

  // Gets the WebContents at an index. Does no bounds checking.
  content::WebContents* GetWebContentsAtImpl(int index) const;

  // Returns the WebContentses at the specified indices. This does no checking
  // of the indices, it is assumed they are valid.
  std::vector<content::WebContents*> GetWebContentsesByIndices(
      const std::vector<int>& indices);

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

  // Sets the selection to |new_model| and notifies any observers.
  // Note: This function might end up sending 0 to 3 notifications in the
  // following order: TabDeactivated, ActiveTabChanged, TabSelectionChanged.
  void SetSelection(ui::ListSelectionModel new_model, Notify notify_types);

  // Selects either the next tab (|forward| is true), or the previous tab
  // (|forward| is false).
  void SelectRelativeTab(bool forward);

  // Does the work of MoveWebContentsAt. This has no checks to make sure the
  // position is valid, those are done in MoveWebContentsAt.
  void MoveWebContentsAtImpl(int index,
                             int to_position,
                             bool select_after_move);

  // Implementation of MoveSelectedTabsTo. Moves |length| of the selected tabs
  // starting at |start| to |index|. See MoveSelectedTabsTo for more details.
  void MoveSelectedTabsToImpl(int index, size_t start, size_t length);

  // Returns true if the tab represented by the specified data has an opener
  // that matches the specified one. If |use_group| is true, then this will
  // fall back to check the group relationship as well.
  static bool OpenerMatches(const std::unique_ptr<WebContentsData>& data,
                            const content::WebContents* opener,
                            bool use_group);

  // Sets the group/opener of any tabs that reference the tab at |index| to that
  // tab's group/opener respectively.
  void FixOpenersAndGroupsReferencing(int index);

  // The WebContents data currently hosted within this TabStripModel.
  std::vector<std::unique_ptr<WebContentsData>> contents_data_;

  // A profile associated with this TabStripModel.
  Profile* profile_;

  // True if all tabs are currently being closed via CloseAllTabs.
  bool closing_all_ = false;

  // An object that determines where new Tabs should be inserted and where
  // selection should move when a Tab is closed.
  std::unique_ptr<TabStripModelOrderController> order_controller_;

  ui::ListSelectionModel selection_model_;

  // Indicates if observers are currently being notified to catch reentrancy
  // bugs. See for example http://crbug.com/529407
  bool in_notify_ = false;

  base::WeakPtrFactory<TabStripModelImpl> weak_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(TabStripModelImpl);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_IMPL_H_
