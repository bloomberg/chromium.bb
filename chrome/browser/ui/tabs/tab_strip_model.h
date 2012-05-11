// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_H_
#pragma once

#include <vector>

#include "base/observer_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/tabs/tab_strip_selection_model.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/common/page_transition_types.h"

class Profile;
class TabContentsWrapper;
class TabStripModelDelegate;
class TabStripModelOrderController;

namespace content {
class NavigationController;
class WebContents;
}

////////////////////////////////////////////////////////////////////////////////
//
// TabStripModel
//
//  A model & low level controller of a Browser Window tabstrip. Holds a vector
//  of TabContentsWrappers, and provides an API for adding, removing and
//  shuffling them, as well as a higher level API for doing specific Browser-
//  related tasks like adding new Tabs from just a URL, etc.
//
// Each tab may be any one of the following states:
// . Mini-tab. Mini tabs are locked to the left side of the tab strip and
//   rendered differently (small tabs with only a favicon). The model makes
//   sure all mini-tabs are at the beginning of the tab strip. For example,
//   if a non-mini tab is added it is forced to be with non-mini tabs. Requests
//   to move tabs outside the range of the tab type are ignored. For example,
//   a request to move a mini-tab after non-mini-tabs is ignored.
//   You'll notice there is no explicit api for making a tab a mini-tab, rather
//   there are two tab types that are implicitly mini-tabs:
//   . App. Corresponds to an extension that wants an app tab. App tabs are
//     identified by TabContentsWrapper::extension_tab_helper()::is_app().
//     App tabs are always pinned (you can't unpin them).
//   . Pinned. Any tab can be pinned. Non-app tabs whose pinned state is changed
//     are moved to be with other mini-tabs or non-mini tabs.
//
//  A TabStripModel has one delegate that it relies on to perform certain tasks
//  like creating new TabStripModels (probably hosted in Browser windows) when
//  required. See TabStripDelegate above for more information.
//
//  A TabStripModel also has N observers (see TabStripModelObserver above),
//  which can be registered via Add/RemoveObserver. An Observer is notified of
//  tab creations, removals, moves, and other interesting events. The
//  TabStrip implements this interface to know when to create new tabs in
//  the View, and the Browser object likewise implements to be able to update
//  its bookkeeping when such events happen.
//
////////////////////////////////////////////////////////////////////////////////
class TabStripModel : public content::NotificationObserver {
 public:
  // Policy for how new tabs are inserted.
  enum InsertionPolicy {
    // Newly created tabs are created after the selection. This is the default.
    INSERT_AFTER,

    // Newly created tabs are inserted before the selection.
    INSERT_BEFORE,
  };

  // Used to specify what should happen when the tab is closed.
  enum CloseTypes {
    CLOSE_NONE                     = 0,

    // Indicates the tab was closed by the user. If true,
    // WebContents::SetClosedByUserGesture(true) is invoked.
    CLOSE_USER_GESTURE             = 1 << 0,

    // If true the history is recorded so that the tab can be reopened later.
    // You almost always want to set this.
    CLOSE_CREATE_HISTORICAL_TAB    = 1 << 1,
  };

  // Constants used when adding tabs.
  enum AddTabTypes {
    // Used to indicate nothing special should happen to the newly inserted
    // tab.
    ADD_NONE          = 0,

    // The tab should be active.
    ADD_ACTIVE        = 1 << 0,

    // The tab should be pinned.
    ADD_PINNED        = 1 << 1,

    // If not set the insertion index of the TabContentsWrapper is left up to
    // the Order Controller associated, so the final insertion index may differ
    // from the specified index. Otherwise the index supplied is used.
    ADD_FORCE_INDEX   = 1 << 2,

    // If set the newly inserted tab inherits the group of the currently
    // selected tab. If not set the tab may still inherit the group under
    // certain situations.
    ADD_INHERIT_GROUP = 1 << 3,

    // If set the newly inserted tab's opener is set to the active tab. If not
    // set the tab may still inherit the group/opener under certain situations.
    // NOTE: this is ignored if ADD_INHERIT_GROUP is set.
    ADD_INHERIT_OPENER = 1 << 4,
  };

  static const int kNoTab = -1;

  // Construct a TabStripModel with a delegate to help it do certain things
  // (See TabStripModelDelegate documentation). |delegate| cannot be NULL.
  TabStripModel(TabStripModelDelegate* delegate, Profile* profile);
  virtual ~TabStripModel();

  // Retrieves the TabStripModelDelegate associated with this TabStripModel.
  TabStripModelDelegate* delegate() const { return delegate_; }

  // Add and remove observers to changes within this TabStripModel.
  void AddObserver(TabStripModelObserver* observer);
  void RemoveObserver(TabStripModelObserver* observer);

  // Retrieve the number of TabContentses/emptiness of the TabStripModel.
  int count() const { return static_cast<int>(contents_data_.size()); }
  bool empty() const { return contents_data_.empty(); }

  // Retrieve the Profile associated with this TabStripModel.
  Profile* profile() const { return profile_; }

  // Retrieve the index of the currently active TabContentsWrapper.
  int active_index() const { return selection_model_.active(); }

  // Returns true if the tabstrip is currently closing all open tabs (via a
  // call to CloseAllTabs). As tabs close, the selection in the tabstrip
  // changes which notifies observers, which can use this as an optimization to
  // avoid doing meaningless or unhelpful work.
  bool closing_all() const { return closing_all_; }

  // Access the order controller. Exposed only for unit tests.
  TabStripModelOrderController* order_controller() const {
    return order_controller_;
  }

  // Sets the insertion policy. Default is INSERT_AFTER.
  void SetInsertionPolicy(InsertionPolicy policy);
  InsertionPolicy insertion_policy() const;

  // Returns true if |observer| is in the list of observers. This is intended
  // for debugging.
  bool HasObserver(TabStripModelObserver* observer);

  // Basic API /////////////////////////////////////////////////////////////////

  // Determines if the specified index is contained within the TabStripModel.
  bool ContainsIndex(int index) const;

  // Adds the specified TabContentsWrapper in the default location. Tabs opened
  // in the foreground inherit the group of the previously active tab.
  void AppendTabContents(TabContentsWrapper* contents, bool foreground);

  // Adds the specified TabContentsWrapper at the specified location.
  // |add_types| is a bitmask of AddTypes; see it for details.
  //
  // All append/insert methods end up in this method.
  //
  // NOTE: adding a tab using this method does NOT query the order controller,
  // as such the ADD_FORCE_INDEX AddType is meaningless here.  The only time the
  // |index| is changed is if using the index would result in breaking the
  // constraint that all mini-tabs occur before non-mini-tabs.
  // See also AddTabContents.
  void InsertTabContentsAt(int index,
                           TabContentsWrapper* contents,
                           int add_types);

  // Closes the TabContentsWrapper at the specified index. This causes the
  // TabContentsWrapper to be destroyed, but it may not happen immediately.
  // |close_types| is a bitmask of CloseTypes. Returns true if the
  // TabContentsWrapper was closed immediately, false if it was not closed (we
  // may be waiting for a response from an onunload handler, or waiting for the
  // user to confirm closure).
  bool CloseTabContentsAt(int index, uint32 close_types);

  // Replaces the entire state of a the tab at index by switching in a
  // different NavigationController. This is used through the recently
  // closed tabs list, which needs to replace a tab's current state
  // and history with another set of contents and history.
  //
  // The old NavigationController is deallocated and this object takes
  // ownership of the passed in controller.
  // XXXPINK This API is weird and wrong. Remove it or change it or rename it?
  void ReplaceNavigationControllerAt(int index,
                                     TabContentsWrapper* contents);

  // Replaces the tab contents at |index| with |new_contents|. The
  // TabContentsWrapper that was at |index| is returned and ownership returns
  // to the caller.
  TabContentsWrapper* ReplaceTabContentsAt(int index,
                                           TabContentsWrapper* new_contents);

  // Destroys the TabContentsWrapper at the specified index, but keeps the tab
  // visible in the tab strip. Used to free memory in low-memory conditions,
  // especially on Chrome OS. The tab reloads if the user clicks on it.
  // Returns an empty TabContentsWrapper, used only for testing.
  TabContentsWrapper* DiscardTabContentsAt(int index);

  // Detaches the TabContentsWrapper at the specified index from this strip. The
  // TabContentsWrapper is not destroyed, just removed from display. The caller
  // is responsible for doing something with it (e.g. stuffing it into another
  // strip).
  TabContentsWrapper* DetachTabContentsAt(int index);

  // Makes the tab at the specified index the active tab. |user_gesture| is true
  // if the user actually clicked on the tab or navigated to it using a keyboard
  // command, false if the tab was activated as a by-product of some other
  // action.
  void ActivateTabAt(int index, bool user_gesture);

  // Adds tab at |index| to the currently selected tabs, without changing the
  // active tab index.
  void AddTabAtToSelection(int index);

  // Move the TabContentsWrapper at the specified index to another index. This
  // method does NOT send Detached/Attached notifications, rather it moves the
  // TabContentsWrapper inline and sends a Moved notification instead.
  // If |select_after_move| is false, whatever tab was selected before the move
  // will still be selected, but it's index may have incremented or decremented
  // one slot.
  // NOTE: this does nothing if the move would result in app tabs and non-app
  // tabs mixing.
  void MoveTabContentsAt(int index, int to_position, bool select_after_move);

  // Moves the selected tabs to |index|. |index| is treated as if the tab strip
  // did not contain any of the selected tabs. For example, if the tabstrip
  // contains [A b c D E f] (upper case selected) and this is invoked with 1 the
  // result is [b A D E c f].
  // This method maintains that all mini-tabs occur before non-mini-tabs.  When
  // mini-tabs are selected the move is processed in two chunks: first mini-tabs
  // are moved, then non-mini-tabs are moved. If the index is after
  // (mini-tab-count - selected-mini-tab-count), then the index the non-mini
  // selected tabs are moved to is (index + selected-mini-tab-count). For
  // example, if the model consists of [A b c D E f] (A b c are mini) and this
  // is inokved with 2, the result is [b c A D E f]. In this example nothing
  // special happened because the target index was <= (mini-tab-count -
  // selected-mini-tab-count). If the target index were 3, then the result would
  // be [b c A f D F]. A, being mini, can move no further than index 2. The
  // non-mini-tabs are moved to the target index + selected-mini-tab-count (3 +
  // 1)
  void MoveSelectedTabsTo(int index);

  // Returns the currently active TabContentsWrapper, or NULL if there is none.
  TabContentsWrapper* GetActiveTabContents() const;

  // Returns the TabContentsWrapper at the specified index, or NULL if there is
  // none.
  TabContentsWrapper* GetTabContentsAt(int index) const;

  // Returns the index of the specified TabContentsWrapper, or
  // TabStripModel::kNoTab if the TabContentsWrapper is not in this
  // TabStripModel.
  int GetIndexOfTabContents(const TabContentsWrapper* contents) const;

  // Returns the index of the specified TabContentsWrapper given its raw
  // WebContents, or TabStripModel::kNoTab if the WebContents is not in this
  // TabStripModel.  Note: This is only needed in rare cases where the wrapper
  // is not already present (such as implementing WebContentsDelegate methods,
  // which don't know about the wrapper.  Returns NULL if |contents| is not
  // associated with any TabContentsWrapper in the model.
  int GetWrapperIndex(const content::WebContents* contents) const;

  // Returns the index of the specified NavigationController, or kNoTab if it is
  // not in this TabStripModel.
  int GetIndexOfController(
      const content::NavigationController* controller) const;

  // Notify any observers that the TabContentsWrapper at the specified index has
  // changed in some way. See TabChangeType for details of |change_type|.
  void UpdateTabContentsStateAt(
      int index,
      TabStripModelObserver::TabChangeType change_type);

  // Make sure there is an auto-generated New Tab tab in the TabStripModel.
  // If |force_create| is true, the New Tab will be created even if the
  // preference is set to false (used by startup).
  void EnsureNewTabVisible(bool force_create);

  // Close all tabs at once. Code can use closing_all() above to defer
  // operations that might otherwise by invoked by the flurry of detach/select
  // notifications this method causes.
  void CloseAllTabs();

  // Returns true if there are any TabContentsWrappers that are currently
  // loading.
  bool TabsAreLoading() const;

  // Returns the controller controller that opened the TabContentsWrapper at
  // |index|.
  content::NavigationController* GetOpenerOfTabContentsAt(int index);

  // Changes the |opener| of the TabContentsWrapper at |index|.
  // Note: |opener| must be in this tab strip.
  void SetOpenerOfTabContentsAt(
      int index,
      content::NavigationController* opener);

  // Returns the index of the next TabContentsWrapper in the sequence of
  // TabContentsWrappers spawned by the specified NavigationController after
  // |start_index|. If |use_group| is true, the group property of the tab is
  // used instead of the opener to find the next tab. Under some circumstances
  // the group relationship may exist but the opener may not.
  int GetIndexOfNextTabContentsOpenedBy(
      const content::NavigationController* opener,
      int start_index,
      bool use_group) const;

  // Returns the index of the first TabContentsWrapper in the model opened by
  // the specified opener.
  int GetIndexOfFirstTabContentsOpenedBy(
      const content::NavigationController* opener,
      int start_index) const;

  // Returns the index of the last TabContentsWrapper in the model opened by the
  // specified opener, starting at |start_index|.
  int GetIndexOfLastTabContentsOpenedBy(
      const content::NavigationController* opener,
      int start_index) const;

  // Called by the Browser when a navigation is about to occur in the specified
  // TabContentsWrapper. Depending on the tab, and the transition type of the
  // navigation, the TabStripModel may adjust its selection and grouping
  // behavior.
  void TabNavigating(TabContentsWrapper* contents,
                     content::PageTransition transition);

  // Forget all Opener relationships that are stored (but _not_ group
  // relationships!) This is to reduce unpredictable tab switching behavior
  // in complex session states. The exact circumstances under which this method
  // is called are left up to the implementation of the selected
  // TabStripModelOrderController.
  void ForgetAllOpeners();

  // Forgets the group affiliation of the specified TabContentsWrapper. This
  // should be called when a TabContentsWrapper that is part of a logical group
  // of tabs is moved to a new logical context by the user (e.g. by typing a new
  // URL or selecting a bookmark). This also forgets the opener, which is
  // considered a weaker relationship than group.
  void ForgetGroup(TabContentsWrapper* contents);

  // Returns true if the group/opener relationships present for |contents|
  // should be reset when _any_ selection change occurs in the model.
  bool ShouldResetGroupOnSelect(TabContentsWrapper* contents) const;

  // Changes the blocked state of the tab at |index|.
  void SetTabBlocked(int index, bool blocked);

  // Changes the pinned state of the tab at |index|. See description above
  // class for details on this.
  void SetTabPinned(int index, bool pinned);

  // Returns true if the tab at |index| is pinned.
  // See description above class for details on pinned tabs.
  bool IsTabPinned(int index) const;

  // Is the tab a mini-tab?
  // See description above class for details on this.
  bool IsMiniTab(int index) const;

  // Is the tab at |index| an app?
  // See description above class for details on app tabs.
  bool IsAppTab(int index) const;

  // Returns true if the tab at |index| is blocked by a tab modal dialog.
  bool IsTabBlocked(int index) const;

  // Returns true if the TabContentsWrapper at |index| has been discarded to
  // save memory.  See DiscardTabContentsAt() for details.
  bool IsTabDiscarded(int index) const;

  // Returns the index of the first tab that is not a mini-tab. This returns
  // |count()| if all of the tabs are mini-tabs, and 0 if none of the tabs are
  // mini-tabs.
  int IndexOfFirstNonMiniTab() const;

  // Returns a valid index for inserting a new tab into this model. |index| is
  // the proposed index and |mini_tab| is true if inserting a tab will become
  // mini (pinned or app). If |mini_tab| is true, the returned index is between
  // 0 and IndexOfFirstNonMiniTab. If |mini_tab| is false, the returned index
  // is between IndexOfFirstNonMiniTab and count().
  int ConstrainInsertionIndex(int index, bool mini_tab);

  // Extends the selection from the anchor to |index|.
  void ExtendSelectionTo(int index);

  // Toggles the selection at |index|. This does nothing if |index| is selected
  // and there are no other selected tabs.
  void ToggleSelectionAt(int index);

  // Makes sure the tabs from the anchor to |index| are selected. This only
  // adds to the selection.
  void AddSelectionFromAnchorTo(int index);

  // Returns true if the tab at |index| is selected.
  bool IsTabSelected(int index) const;

  // Sets the selection to match that of |source|.
  void SetSelectionFromModel(const TabStripSelectionModel& source);

  const TabStripSelectionModel& selection_model() const {
    return selection_model_;
  }

  // Command level API /////////////////////////////////////////////////////////

  // Adds a TabContentsWrapper at the best position in the TabStripModel given
  // the specified insertion index, transition, etc. |add_types| is a bitmask of
  // AddTypes; see it for details. This method ends up calling into
  // InsertTabContentsAt to do the actual insertion.
  void AddTabContents(TabContentsWrapper* contents,
                      int index,
                      content::PageTransition transition,
                      int add_types);

  // Closes the selected tabs.
  void CloseSelectedTabs();

  // Select adjacent tabs
  void SelectNextTab();
  void SelectPreviousTab();

  // Selects the last tab in the tab strip.
  void SelectLastTab();

  // Swap adjacent tabs.
  void MoveTabNext();
  void MoveTabPrevious();

  // Notifies the observers that the active/foreground tab at |index| was
  // reselected (ie - it was already active and was clicked again).
  void ActiveTabClicked(int index);

  // View API //////////////////////////////////////////////////////////////////

  // Context menu functions.
  enum ContextMenuCommand {
    CommandFirst = 0,
    CommandNewTab,
    CommandReload,
    CommandDuplicate,
    CommandCloseTab,
    CommandCloseOtherTabs,
    CommandCloseTabsToRight,
    CommandRestoreTab,
    CommandTogglePinned,
    CommandBookmarkAllTabs,
    CommandUseCompactNavigationBar,
    CommandSelectByDomain,
    CommandSelectByOpener,
    CommandLast
  };

  // Returns true if the specified command is enabled. If |context_index| is
  // selected the response applies to all selected tabs.
  bool IsContextMenuCommandEnabled(int context_index,
                                   ContextMenuCommand command_id) const;

  // Performs the action associated with the specified command for the given
  // TabStripModel index |context_index|.  If |context_index| is selected the
  // command applies to all selected tabs.
  void ExecuteContextMenuCommand(int context_index,
                                 ContextMenuCommand command_id);

  // Returns a vector of indices of the tabs that will close when executing the
  // command |id| for the tab at |index|. The returned indices are sorted in
  // descending order.
  std::vector<int> GetIndicesClosedByCommand(int index,
                                             ContextMenuCommand id) const;

  // Returns true if 'CommandTogglePinned' will pin. |index| is the index
  // supplied to |ExecuteContextMenuCommand|.
  bool WillContextMenuPin(int index);

  // Overridden from content::NotificationObserver:
  virtual void Observe(int type,
                       const content::NotificationSource& source,
                       const content::NotificationDetails& details) OVERRIDE;

  // Convert a ContextMenuCommand into a browser command. Returns true if a
  // corresponding browser command exists, false otherwise.
  static bool ContextMenuCommandToBrowserCommand(int cmd_id, int* browser_cmd);

 private:
   // Used when making selection notifications.
  enum NotifyTypes {
    NOTIFY_DEFAULT,

    // The selection is changing from a user gesture.
    NOTIFY_USER_GESTURE,
  };

  // Gets the set of tab indices whose domain matches the tab at |index|.
  void GetIndicesWithSameDomain(int index, std::vector<int>* indices);

  // Gets the set of tab indices that have the same opener as the tab at
  // |index|.
  void GetIndicesWithSameOpener(int index, std::vector<int>* indices);

  // If |index| is selected all the selected indices are returned, otherwise a
  // vector with |index| is returned. This is used when executing commands to
  // determine which indices the command applies to.
  std::vector<int> GetIndicesForCommand(int index) const;

  // Returns true if the specified TabContentsWrapper is a New Tab at the end of
  // the TabStrip. We check for this because opener relationships are _not_
  // forgotten for the New Tab page opened as a result of a New Tab gesture
  // (e.g. Ctrl+T, etc) since the user may open a tab transiently to look up
  // something related to their current activity.
  bool IsNewTabAtEndOfTabStrip(TabContentsWrapper* contents) const;

  // Closes the TabContentsWrappers at the specified indices. This causes the
  // TabContentsWrappers to be destroyed, but it may not happen immediately. If
  // the page in question has an unload event the WebContents will not be
  // destroyed until after the event has completed, which will then call back
  // into this method.
  //
  // Returns true if the TabContentsWrapper were closed immediately, false if we
  // are waiting for the result of an onunload handler.
  bool InternalCloseTabs(const std::vector<int>& in_indices,
                         uint32 close_types);

  // Invoked from InternalCloseTabs and when an extension is removed for an app
  // tab. Notifies observers of TabClosingAt and deletes |contents|. If
  // |create_historical_tabs| is true, CreateHistoricalTab is invoked on the
  // delegate.
  //
  // The boolean parameter create_historical_tab controls whether to
  // record these tabs and their history for reopening recently closed
  // tabs.
  void InternalCloseTab(TabContentsWrapper* contents,
                        int index,
                        bool create_historical_tabs);

  TabContentsWrapper* GetContentsAt(int index) const;

  // Notifies the observers if the active tab is being deactivated.
  void NotifyIfTabDeactivated(TabContentsWrapper* contents);

  // Notifies the observers if the active tab has changed.
  void NotifyIfActiveTabChanged(TabContentsWrapper* old_contents,
                                NotifyTypes notify_types);

  // Notifies the observers if the active tab or the tab selection has changed.
  // |old_model| is a snapshot of |selection_model_| before the change.
  // Note: This function might end up sending 0 to 2 notifications in the
  // following order: ActiveTabChanged, TabSelectionChanged.
  void NotifyIfActiveOrSelectionChanged(
      TabContentsWrapper* old_contents,
      NotifyTypes notify_types,
      const TabStripSelectionModel& old_model);

  // Sets the selection to |new_model| and notifies any observers.
  // Note: This function might end up sending 0 to 3 notifications in the
  // following order: TabDeactivated, ActiveTabChanged, TabSelectionChanged.
  void SetSelection(const TabStripSelectionModel& new_model,
                    NotifyTypes notify_types);

  // Returns the number of New Tab tabs in the TabStripModel.
  int GetNewTabCount() const;

  // Selects either the next tab (|foward| is true), or the previous tab
  // (|forward| is false).
  void SelectRelativeTab(bool forward);

  // Does the work of MoveTabContentsAt. This has no checks to make sure the
  // position is valid, those are done in MoveTabContentsAt.
  void MoveTabContentsAtImpl(int index,
                             int to_position,
                             bool select_after_move);

  // Implementation of MoveSelectedTabsTo. Moves |length| of the selected tabs
  // starting at |start| to |index|. See MoveSelectedTabsTo for more details.
  void MoveSelectedTabsToImpl(int index, size_t start, size_t length);

  // Returns true if the tab represented by the specified data has an opener
  // that matches the specified one. If |use_group| is true, then this will
  // fall back to check the group relationship as well.
  struct TabContentsData;
  static bool OpenerMatches(const TabContentsData* data,
                            const content::NavigationController* opener,
                            bool use_group);

  // Sets the group/opener of any tabs that reference |tab| to NULL.
  void ForgetOpenersAndGroupsReferencing(
      const content::NavigationController* tab);

  // Our delegate.
  TabStripModelDelegate* delegate_;

  // A hunk of data representing a TabContentsWrapper and (optionally) the
  // NavigationController that spawned it. This memory only sticks around while
  // the TabContentsWrapper is in the current TabStripModel, unless otherwise
  // specified in code.
  struct TabContentsData {
    explicit TabContentsData(TabContentsWrapper* a_contents)
        : contents(a_contents),
          reset_group_on_select(false),
          pinned(false),
          blocked(false),
          discarded(false) {
      SetGroup(NULL);
    }

    // Create a relationship between this TabContentsWrapper and other
    // TabContentsWrappers. Used to identify which TabContentsWrapper to select
    // next after one is closed.
    void SetGroup(content::NavigationController* a_group) {
      group = a_group;
      opener = a_group;
    }

    // Forget the opener relationship so that when this TabContentsWrapper is
    // closed unpredictable re-selection does not occur.
    void ForgetOpener() {
      opener = NULL;
    }

    TabContentsWrapper* contents;
    // We use NavigationControllers here since they more closely model the
    // "identity" of a Tab, TabContentsWrappers can change depending on the URL
    // loaded in the Tab.
    // The group is used to model a set of tabs spawned from a single parent
    // tab. This value is preserved for a given tab as long as the tab remains
    // navigated to the link it was initially opened at or some navigation from
    // that page (i.e. if the user types or visits a bookmark or some other
    // navigation within that tab, the group relationship is lost). This
    // property can safely be used to implement features that depend on a
    // logical group of related tabs.
    content::NavigationController* group;
    // The owner models the same relationship as group, except it is more
    // easily discarded, e.g. when the user switches to a tab not part of the
    // same group. This property is used to determine what tab to select next
    // when one is closed.
    content::NavigationController* opener;
    // True if our group should be reset the moment selection moves away from
    // this Tab. This is the case for tabs opened in the foreground at the end
    // of the TabStrip while viewing another Tab. If these tabs are closed
    // before selection moves elsewhere, their opener is selected. But if
    // selection shifts to _any_ tab (including their opener), the group
    // relationship is reset to avoid confusing close sequencing.
    bool reset_group_on_select;

    // Is the tab pinned?
    bool pinned;

    // Is the tab interaction blocked by a modal dialog?
    bool blocked;

    // Has the tab data been discarded to save memory?
    bool discarded;
  };

  // The TabContentsWrapper data currently hosted within this TabStripModel.
  typedef std::vector<TabContentsData*> TabContentsDataVector;
  TabContentsDataVector contents_data_;

  // A profile associated with this TabStripModel, used when creating new Tabs.
  Profile* profile_;

  // True if all tabs are currently being closed via CloseAllTabs.
  bool closing_all_;

  // An object that determines where new Tabs should be inserted and where
  // selection should move when a Tab is closed.
  TabStripModelOrderController* order_controller_;

  // Our observers.
  typedef ObserverList<TabStripModelObserver> TabStripModelObservers;
  TabStripModelObservers observers_;

  // A scoped container for notification registries.
  content::NotificationRegistrar registrar_;

  TabStripSelectionModel selection_model_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(TabStripModel);
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_H_
