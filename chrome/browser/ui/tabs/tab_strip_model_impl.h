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
#include "base/observer_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
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

  // Retrieves the TabStripModelDelegate associated with this TabStripModel.
  TabStripModelDelegate* delegate() const override;

  // Add and remove observers to changes within this TabStripModel.
  void AddObserver(TabStripModelObserver* observer) override;
  void RemoveObserver(TabStripModelObserver* observer) override;

  // Retrieve the number of WebContentses/emptiness of the TabStripModel.
  int count() const override;
  bool empty() const override;

  // Retrieve the Profile associated with this TabStripModel.
  Profile* profile() const override;

  // Retrieve the index of the currently active WebContents. This will be
  // ui::ListSelectionModel::kUnselectedIndex if no tab is currently selected
  // (this happens while the tab strip is being initialized or is empty).
  int active_index() const override;

  // Returns true if the tabstrip is currently closing all open tabs (via a
  // call to CloseAllTabs). As tabs close, the selection in the tabstrip
  // changes which notifies observers, which can use this as an optimization to
  // avoid doing meaningless or unhelpful work.
  bool closing_all() const override;

  // Access the order controller. Exposed only for unit tests.
  TabStripModelOrderController* order_controller() const override;

  // Basic API /////////////////////////////////////////////////////////////////

  // Determines if the specified index is contained within the TabStripModel.
  bool ContainsIndex(int index) const override;

  // Adds the specified WebContents in the default location. Tabs opened
  // in the foreground inherit the group of the previously active tab.
  void AppendWebContents(content::WebContents* contents,
                         bool foreground) override;

  // Adds the specified WebContents at the specified location.
  // |add_types| is a bitmask of AddTabTypes override; see it for details.
  //
  // All append/insert methods end up in this method.
  //
  // NOTE: adding a tab using this method does NOT query the order controller,
  // as such the ADD_FORCE_INDEX AddTabTypes is meaningless here.  The only time
  // the |index| is changed is if using the index would result in breaking the
  // constraint that all pinned tabs occur before non-pinned tabs.
  // See also AddWebContents.
  void InsertWebContentsAt(int index,
                           content::WebContents* contents,
                           int add_types) override;

  // Closes the WebContents at the specified index. This causes the
  // WebContents to be destroyed, but it may not happen immediately.
  // |close_types| is a bitmask of CloseTypes. Returns true if the
  // WebContents was closed immediately, false if it was not closed (we
  // may be waiting for a response from an onunload handler, or waiting for the
  // user to confirm closure).
  bool CloseWebContentsAt(int index, uint32_t close_types) override;

  // Replaces the WebContents at |index| with |new_contents|. The
  // WebContents that was at |index| is returned and its ownership returns
  // to the caller.
  content::WebContents* ReplaceWebContentsAt(
      int index,
      content::WebContents* new_contents) override;

  // Detaches the WebContents at the specified index from this strip. The
  // WebContents is not destroyed, just removed from display. The caller
  // is responsible for doing something with it (e.g. stuffing it into another
  // strip). Returns the detached WebContents.
  content::WebContents* DetachWebContentsAt(int index) override;

  // Makes the tab at the specified index the active tab. |user_gesture| is true
  // if the user actually clicked on the tab or navigated to it using a keyboard
  // command, false if the tab was activated as a by-product of some other
  // action.
  void ActivateTabAt(int index, bool user_gesture) override;

  // Adds tab at |index| to the currently selected tabs, without changing the
  // active tab index.
  void AddTabAtToSelection(int index) override;

  // Move the WebContents at the specified index to another index. This
  // method does NOT send Detached/Attached notifications, rather it moves the
  // WebContents inline and sends a Moved notification instead.
  // If |select_after_move| is false, whatever tab was selected before the move
  // will still be selected, but its index may have incremented or decremented
  // one slot.
  void MoveWebContentsAt(int index,
                         int to_position,
                         bool select_after_move) override;

  // Moves the selected tabs to |index|. |index| is treated as if the tab strip
  // did not contain any of the selected tabs. For example, if the tabstrip
  // contains [A b c D E f] (upper case selected) and this is invoked with 1 the
  // result is [b A D E c f].
  // This method maintains that all pinned tabs occur before non-pinned tabs.
  // When pinned tabs are selected the move is processed in two chunks: first
  // pinned tabs are moved, then non-pinned tabs are moved. If the index is
  // after (pinned-tab-count - selected-pinned-tab-count), then the index the
  // non-pinned selected tabs are moved to is (index +
  // selected-pinned-tab-count). For example, if the model consists of
  // [A b c D E f] (A b c are pinned) and this is invoked with 2, the result is
  // [b c A D E f]. In this example nothing special happened because the target
  // index was <= (pinned-tab-count - selected-pinned-tab-count). If the target
  // index were 3, then the result would be [b c A f D F]. A, being pinned, can
  // move no further than index 2. The non-pinned tabs are moved to the target
  // index + selected-pinned tab-count (3 + 1).
  void MoveSelectedTabsTo(int index) override;

  // Returns the currently active WebContents, or NULL if there is none.
  content::WebContents* GetActiveWebContents() const override;

  // Returns the WebContents at the specified index, or NULL if there is
  // none.
  content::WebContents* GetWebContentsAt(int index) const override;

  // Returns the index of the specified WebContents, or TabStripModel::kNoTab
  // if the WebContents is not in this TabStripModel.
  int GetIndexOfWebContents(
      const content::WebContents* contents) const override;

  // Notify any observers that the WebContents at the specified index has
  // changed in some way. See TabChangeType for details of |change_type|.
  void UpdateWebContentsStateAt(
      int index,
      TabStripModelObserver::TabChangeType change_type) override;

  // Cause a tab to display a UI indication to the user that it needs their
  // attention.
  void SetTabNeedsAttentionAt(int index, bool attention) override;

  // Close all tabs at once. Code can use closing_all() above to defer
  // operations that might otherwise by invoked by the flurry of detach/select
  // notifications this method causes.
  void CloseAllTabs() override;

  // Returns true if there are any WebContentses that are currently loading.
  bool TabsAreLoading() const override;

  // Returns the WebContents that opened the WebContents at |index|, or NULL if
  // there is no opener on record.
  content::WebContents* GetOpenerOfWebContentsAt(int index) override;

  // Changes the |opener| of the WebContents at |index|.
  // Note: |opener| must be in this tab strip.
  void SetOpenerOfWebContentsAt(int index,
                                content::WebContents* opener) override;

  // Returns the index of the next WebContents in the sequence of WebContentses
  // spawned by the specified WebContents after |start_index|. If |use_group| is
  // true, the group property of the tab is used instead of the opener to find
  // the next tab. Under some circumstances the group relationship may exist but
  // the opener may not.
  int GetIndexOfNextWebContentsOpenedBy(const content::WebContents* opener,
                                        int start_index,
                                        bool use_group) const override;

  // Returns the index of the last WebContents in the model opened by the
  // specified opener, starting at |start_index|.
  int GetIndexOfLastWebContentsOpenedBy(const content::WebContents* opener,
                                        int start_index) const override;

  // To be called when a navigation is about to occur in the specified
  // WebContents. Depending on the tab, and the transition type of the
  // navigation, the TabStripModel may adjust its selection and grouping
  // behavior.
  void TabNavigating(content::WebContents* contents,
                     ui::PageTransition transition) override;

  // Forget all Opener relationships that are stored (but _not_ group
  // relationships!) This is to reduce unpredictable tab switching behavior
  // in complex session states. The exact circumstances under which this method
  // is called are left up to the implementation of the selected
  // TabStripModelOrderController.
  void ForgetAllOpeners() override;

  // Forgets the group affiliation of the specified WebContents. This
  // should be called when a WebContents that is part of a logical group
  // of tabs is moved to a new logical context by the user (e.g. by typing a new
  // URL or selecting a bookmark). This also forgets the opener, which is
  // considered a weaker relationship than group.
  void ForgetGroup(content::WebContents* contents) override;

  // Returns true if the group/opener relationships present for |contents|
  // should be reset when _any_ selection change occurs in the model.
  bool ShouldResetGroupOnSelect(content::WebContents* contents) const override;

  // Changes the blocked state of the tab at |index|.
  void SetTabBlocked(int index, bool blocked) override;

  // Changes the pinned state of the tab at |index|. See description above
  // class for details on this.
  void SetTabPinned(int index, bool pinned) override;

  // Returns true if the tab at |index| is pinned.
  // See description above class for details on pinned tabs.
  bool IsTabPinned(int index) const override;

  // Returns true if the tab at |index| is blocked by a tab modal dialog.
  bool IsTabBlocked(int index) const override;

  // Returns the index of the first tab that is not a pinned tab. This returns
  // |count()| if all of the tabs are pinned tabs, and 0 if none of the tabs are
  // pinned tabs.
  int IndexOfFirstNonPinnedTab() const override;

  // Returns a valid index for inserting a new tab into this model. |index| is
  // the proposed index and |pinned_tab| is true if inserting a tab will become
  // pinned (pinned). If |pinned_tab| is true, the returned index is between 0
  // and IndexOfFirstNonPinnedTab. If |pinned_tab| is false, the returned index
  // is between IndexOfFirstNonPinnedTab and count().
  int ConstrainInsertionIndex(int index, bool pinned_tab) override;

  // Extends the selection from the anchor to |index|.
  void ExtendSelectionTo(int index) override;

  // Toggles the selection at |index|. This does nothing if |index| is selected
  // and there are no other selected tabs.
  void ToggleSelectionAt(int index) override;

  // Makes sure the tabs from the anchor to |index| are selected. This only
  // adds to the selection.
  void AddSelectionFromAnchorTo(int index) override;

  // Returns true if the tab at |index| is selected.
  bool IsTabSelected(int index) const override;

  // Sets the selection to match that of |source|.
  void SetSelectionFromModel(ui::ListSelectionModel source) override;

  const ui::ListSelectionModel& selection_model() const override;

  // Command level API /////////////////////////////////////////////////////////

  // Adds a WebContents at the best position in the TabStripModel given
  // the specified insertion index, transition, etc. |add_types| is a bitmask of
  // AddTabTypes; see it for details. This method ends up calling into
  // InsertWebContentsAt to do the actual insertion. Pass kNoTab for |index| to
  // append the contents to the end of the tab strip.
  void AddWebContents(content::WebContents* contents,
                      int index,
                      ui::PageTransition transition,
                      int add_types) override;

  // Closes the selected tabs.
  void CloseSelectedTabs() override;

  // Select adjacent tabs
  void SelectNextTab() override;
  void SelectPreviousTab() override;

  // Selects the last tab in the tab strip.
  void SelectLastTab() override;

  // Swap adjacent tabs.
  void MoveTabNext() override;
  void MoveTabPrevious() override;

  // View API //////////////////////////////////////////////////////////////////

  // Returns true if the specified command is enabled. If |context_index| is
  // selected the response applies to all selected tabs.
  bool IsContextMenuCommandEnabled(
      int context_index,
      ContextMenuCommand command_id) const override;

  // Performs the action associated with the specified command for the given
  // TabStripModel index |context_index|.  If |context_index| is selected the
  // command applies to all selected tabs.
  void ExecuteContextMenuCommand(int context_index,
                                 ContextMenuCommand command_id) override;

  // Returns a vector of indices of the tabs that will close when executing the
  // command |id| for the tab at |index|. The returned indices are sorted in
  // descending order.
  std::vector<int> GetIndicesClosedByCommand(int index, ContextMenuCommand id)
      const override;

  // Returns true if 'CommandToggleTabAudioMuted' will mute. |index| is the
  // index supplied to |ExecuteContextMenuCommand|.
  bool WillContextMenuMute(int index) override;

  // Returns true if 'CommandToggleSiteMuted' will mute. |index| is the
  // index supplied to |ExecuteContextMenuCommand|.
  bool WillContextMenuMuteSites(int index) override;

  // Returns true if 'CommandTogglePinned' will pin. |index| is the index
  // supplied to |ExecuteContextMenuCommand|.
  bool WillContextMenuPin(int index) override;

 private:
  class WebContentsData;

  // Used when making selection notifications.
  enum NotifyTypes {
    NOTIFY_DEFAULT,

    // The selection is changing from a user gesture.
    NOTIFY_USER_GESTURE,
  };

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
  bool InternalCloseTabs(const std::vector<int>& indices, uint32_t close_types);

  // Invoked from InternalCloseTabs and when an extension is removed for an app
  // tab. Notifies observers of TabClosingAt and deletes |contents|. If
  // |create_historical_tabs| is true, CreateHistoricalTab is invoked on the
  // delegate.
  //
  // The boolean parameter create_historical_tab controls whether to
  // record these tabs and their history for reopening recently closed
  // tabs.
  void InternalCloseTab(content::WebContents* contents,
                        int index,
                        bool create_historical_tabs);

  // Gets the WebContents at an index. Does no bounds checking.
  content::WebContents* GetWebContentsAtImpl(int index) const;

  // Notifies the observers if the active tab is being deactivated.
  void NotifyIfTabDeactivated(content::WebContents* contents);

  // Notifies the observers if the active tab has changed.
  void NotifyIfActiveTabChanged(content::WebContents* old_contents,
                                NotifyTypes notify_types);

  // Notifies the observers if the active tab or the tab selection has changed.
  // |old_model| is a snapshot of |selection_model_| before the change.
  // Note: This function might end up sending 0 to 2 notifications in the
  // following order: ActiveTabChanged, TabSelectionChanged.
  void NotifyIfActiveOrSelectionChanged(
      content::WebContents* old_contents,
      NotifyTypes notify_types,
      const ui::ListSelectionModel& old_model);

  // Sets the selection to |new_model| and notifies any observers.
  // Note: This function might end up sending 0 to 3 notifications in the
  // following order: TabDeactivated, ActiveTabChanged, TabSelectionChanged.
  void SetSelection(ui::ListSelectionModel new_model, NotifyTypes notify_types);

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

  // Our delegate.
  TabStripModelDelegate* delegate_;

  // The WebContents data currently hosted within this TabStripModel.
  std::vector<std::unique_ptr<WebContentsData>> contents_data_;

  // A profile associated with this TabStripModel.
  Profile* profile_;

  // True if all tabs are currently being closed via CloseAllTabs.
  bool closing_all_ = false;

  // An object that determines where new Tabs should be inserted and where
  // selection should move when a Tab is closed.
  std::unique_ptr<TabStripModelOrderController> order_controller_;

  // Our observers.
  base::ObserverList<TabStripModelObserver> observers_;

  ui::ListSelectionModel selection_model_;

  // Indicates if observers are currently being notified to catch reentrancy
  // bugs. See for example http://crbug.com/529407
  bool in_notify_ = false;

  base::WeakPtrFactory<TabStripModelImpl> weak_factory_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(TabStripModelImpl);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_IMPL_H_
