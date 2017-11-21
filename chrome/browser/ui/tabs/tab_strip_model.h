// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_H_
#define CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_H_

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/macros.h"
#include "base/observer_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "chrome/browser/ui/tabs/web_contents_closer.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/page_transition_types.h"

class Profile;
class TabStripModelDelegate;
class TabStripModelExperimental;

namespace content {
class WebContents;
}

////////////////////////////////////////////////////////////////////////////////
//
// TabStripModel
//
// A model & low level controller of a Browser Window tabstrip. Holds a vector
// of WebContentses, and provides an API for adding, removing and
// shuffling them, as well as a higher level API for doing specific Browser-
// related tasks like adding new Tabs from just a URL, etc.
//
// Each tab may be pinned. Pinned tabs are locked to the left side of the tab
// strip and rendered differently (small tabs with only a favicon). The model
// makes sure all pinned tabs are at the beginning of the tab strip. For
// example, if a non-pinned tab is added it is forced to be with non-pinned
// tabs. Requests to move tabs outside the range of the tab type are ignored.
// For example, a request to move a pinned tab after non-pinned tabs is ignored.
//
// A TabStripModel has one delegate that it relies on to perform certain tasks
// like creating new TabStripModels (probably hosted in Browser windows) when
// required. See TabStripDelegate above for more information.
//
// A TabStripModel also has N observers (see TabStripModelObserver above),
// which can be registered via Add/RemoveObserver. An Observer is notified of
// tab creations, removals, moves, and other interesting events. The
// TabStrip implements this interface to know when to create new tabs in
// the View, and the Browser object likewise implements to be able to update
// its bookkeeping when such events happen.
//
////////////////////////////////////////////////////////////////////////////////
class TabStripModel : public WebContentsCloseDelegate {
 public:
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

    // If not set the insertion index of the WebContents is left up to
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

  // Enumerates different ways to open a new tab. Does not apply to opening
  // existing links or searches in a new tab, only to brand new empty tabs.
  enum NewTab {
    // New tab was opened using the new tab button on the tab strip.
    NEW_TAB_BUTTON,

    // New tab was opened using the menu command - either through the keyboard
    // shortcut, or by opening the menu and selecting the command. Applies to
    // both app menu and the menu bar's File menu (on platforms that have one).
    NEW_TAB_COMMAND,

    // New tab was opened through the context menu on the tab strip.
    NEW_TAB_CONTEXT_MENU,

    // Number of enum entries, used for UMA histogram reporting macros.
    NEW_TAB_ENUM_COUNT,
  };

  static const int kNoTab = -1;

  explicit TabStripModel(TabStripModelDelegate* delegate);
  ~TabStripModel() override;

  // Returns the experimental implementation if there is one, nullptr otherwise.
  virtual TabStripModelExperimental* AsTabStripModelExperimental() = 0;

  // Retrieves the TabStripModelDelegate associated with this TabStripModel.
  TabStripModelDelegate* delegate() const { return delegate_; }

  // Add and remove observers to changes within this TabStripModel.
  void AddObserver(TabStripModelObserver* observer);
  void RemoveObserver(TabStripModelObserver* observer);

  // Retrieve the number of WebContentses/emptiness of the TabStripModel.
  virtual int count() const = 0;
  virtual bool empty() const = 0;

  // Retrieve the Profile associated with this TabStripModel.
  virtual Profile* profile() const = 0;

  // Retrieve the index of the currently active WebContents. This will be
  // ui::ListSelectionModel::kUnselectedIndex if no tab is currently selected
  // (this happens while the tab strip is being initialized or is empty).
  virtual int active_index() const = 0;

  // Returns true if the tabstrip is currently closing all open tabs (via a
  // call to CloseAllTabs). As tabs close, the selection in the tabstrip
  // changes which notifies observers, which can use this as an optimization to
  // avoid doing meaningless or unhelpful work.
  virtual bool closing_all() const = 0;

  // Basic API /////////////////////////////////////////////////////////////////

  // Determines if the specified index is contained within the TabStripModel.
  virtual bool ContainsIndex(int index) const = 0;

  // Adds the specified WebContents in the default location. Tabs opened
  // in the foreground inherit the group of the previously active tab.
  virtual void AppendWebContents(content::WebContents* contents,
                                 bool foreground) = 0;

  // Adds the specified WebContents at the specified location.
  // |add_types| is a bitmask of AddTabTypes; see it for details.
  //
  // All append/insert methods end up in this method.
  //
  // NOTE: adding a tab using this method does NOT query the order controller,
  // as such the ADD_FORCE_INDEX AddTabTypes is meaningless here.  The only time
  // the |index| is changed is if using the index would result in breaking the
  // constraint that all pinned tabs occur before non-pinned tabs.
  // See also AddWebContents.
  virtual void InsertWebContentsAt(int index,
                                   content::WebContents* contents,
                                   int add_types) = 0;

  // Closes the WebContents at the specified index. This causes the
  // WebContents to be destroyed, but it may not happen immediately.
  // |close_types| is a bitmask of CloseTypes. Returns true if the
  // WebContents was closed immediately, false if it was not closed (we
  // may be waiting for a response from an onunload handler, or waiting for the
  // user to confirm closure).
  virtual bool CloseWebContentsAt(int index, uint32_t close_types) = 0;

  // Replaces the WebContents at |index| with |new_contents|. The
  // WebContents that was at |index| is returned and its ownership returns
  // to the caller.
  virtual content::WebContents* ReplaceWebContentsAt(
      int index,
      content::WebContents* new_contents) = 0;

  // Detaches the WebContents at the specified index from this strip. The
  // WebContents is not destroyed, just removed from display. The caller
  // is responsible for doing something with it (e.g. stuffing it into another
  // strip). Returns the detached WebContents.
  virtual content::WebContents* DetachWebContentsAt(int index) = 0;

  // Makes the tab at the specified index the active tab. |user_gesture| is true
  // if the user actually clicked on the tab or navigated to it using a keyboard
  // command, false if the tab was activated as a by-product of some other
  // action.
  virtual void ActivateTabAt(int index, bool user_gesture) = 0;

  // Adds tab at |index| to the currently selected tabs, without changing the
  // active tab index.
  virtual void AddTabAtToSelection(int index) = 0;

  // Move the WebContents at the specified index to another index. This
  // method does NOT send Detached/Attached notifications, rather it moves the
  // WebContents inline and sends a Moved notification instead.
  // If |select_after_move| is false, whatever tab was selected before the move
  // will still be selected, but its index may have incremented or decremented
  // one slot.
  virtual void MoveWebContentsAt(int index,
                                 int to_position,
                                 bool select_after_move) = 0;

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
  virtual void MoveSelectedTabsTo(int index) = 0;

  // Returns the currently active WebContents, or NULL if there is none.
  virtual content::WebContents* GetActiveWebContents() const = 0;

  // Returns the WebContents at the specified index, or NULL if there is
  // none.
  virtual content::WebContents* GetWebContentsAt(int index) const = 0;

  // Returns the index of the specified WebContents, or TabStripModel::kNoTab
  // if the WebContents is not in this TabStripModel.
  virtual int GetIndexOfWebContents(
      const content::WebContents* contents) const = 0;

  // Notify any observers that the WebContents at the specified index has
  // changed in some way. See TabChangeType for details of |change_type|.
  virtual void UpdateWebContentsStateAt(
      int index,
      TabStripModelObserver::TabChangeType change_type) = 0;

  // Cause a tab to display a UI indication to the user that it needs their
  // attention.
  virtual void SetTabNeedsAttentionAt(int index, bool attention) = 0;

  // Close all tabs at once. Code can use closing_all() above to defer
  // operations that might otherwise by invoked by the flurry of detach/select
  // notifications this method causes.
  virtual void CloseAllTabs() = 0;

  // Returns true if there are any WebContentses that are currently loading.
  virtual bool TabsAreLoading() const = 0;

  // Returns the WebContents that opened the WebContents at |index|, or NULL if
  // there is no opener on record.
  virtual content::WebContents* GetOpenerOfWebContentsAt(int index) = 0;

  // Changes the |opener| of the WebContents at |index|.
  // Note: |opener| must be in this tab strip.
  virtual void SetOpenerOfWebContentsAt(int index,
                                        content::WebContents* opener) = 0;

  // Returns the index of the last WebContents in the model opened by the
  // specified opener, starting at |start_index|.
  virtual int GetIndexOfLastWebContentsOpenedBy(
      const content::WebContents* opener,
      int start_index) const = 0;

  // To be called when a navigation is about to occur in the specified
  // WebContents. Depending on the tab, and the transition type of the
  // navigation, the TabStripModel may adjust its selection and grouping
  // behavior.
  virtual void TabNavigating(content::WebContents* contents,
                             ui::PageTransition transition) = 0;

  // Changes the blocked state of the tab at |index|.
  virtual void SetTabBlocked(int index, bool blocked) = 0;

  // Changes the pinned state of the tab at |index|. See description above
  // class for details on this.
  virtual void SetTabPinned(int index, bool pinned) = 0;

  // Returns true if the tab at |index| is pinned.
  // See description above class for details on pinned tabs.
  virtual bool IsTabPinned(int index) const = 0;

  // Returns true if the tab at |index| is blocked by a tab modal dialog.
  virtual bool IsTabBlocked(int index) const = 0;

  // Returns the index of the first tab that is not a pinned tab. This returns
  // |count()| if all of the tabs are pinned tabs, and 0 if none of the tabs are
  // pinned tabs.
  virtual int IndexOfFirstNonPinnedTab() const = 0;

  // Extends the selection from the anchor to |index|.
  virtual void ExtendSelectionTo(int index) = 0;

  // Toggles the selection at |index|. This does nothing if |index| is selected
  // and there are no other selected tabs.
  virtual void ToggleSelectionAt(int index) = 0;

  // Makes sure the tabs from the anchor to |index| are selected. This only
  // adds to the selection.
  virtual void AddSelectionFromAnchorTo(int index) = 0;

  // Returns true if the tab at |index| is selected.
  virtual bool IsTabSelected(int index) const = 0;

  // Sets the selection to match that of |source|.
  virtual void SetSelectionFromModel(ui::ListSelectionModel source) = 0;

  virtual const ui::ListSelectionModel& selection_model() const = 0;

  // Command level API /////////////////////////////////////////////////////////

  // Adds a WebContents at the best position in the TabStripModel given
  // the specified insertion index, transition, etc. |add_types| is a bitmask of
  // AddTabTypes; see it for details. This method ends up calling into
  // InsertWebContentsAt to do the actual insertion. Pass kNoTab for |index| to
  // append the contents to the end of the tab strip.
  virtual void AddWebContents(content::WebContents* contents,
                              int index,
                              ui::PageTransition transition,
                              int add_types) = 0;

  // Closes the selected tabs.
  virtual void CloseSelectedTabs() = 0;

  // Select adjacent tabs
  virtual void SelectNextTab() = 0;
  virtual void SelectPreviousTab() = 0;

  // Selects the last tab in the tab strip.
  virtual void SelectLastTab() = 0;

  // Swap adjacent tabs.
  virtual void MoveTabNext() = 0;
  virtual void MoveTabPrevious() = 0;

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
    CommandToggleTabAudioMuted,
    CommandToggleSiteMuted,
    CommandBookmarkAllTabs,
    CommandSelectByDomain,
    CommandSelectByOpener,
    CommandLast
  };

  // Returns true if the specified command is enabled. If |context_index| is
  // selected the response applies to all selected tabs.
  virtual bool IsContextMenuCommandEnabled(
      int context_index,
      ContextMenuCommand command_id) const = 0;

  // Performs the action associated with the specified command for the given
  // TabStripModel index |context_index|.  If |context_index| is selected the
  // command applies to all selected tabs.
  virtual void ExecuteContextMenuCommand(int context_index,
                                         ContextMenuCommand command_id) = 0;

  // Returns a vector of indices of the tabs that will close when executing the
  // command |id| for the tab at |index|. The returned indices are sorted in
  // descending order.
  virtual std::vector<int> GetIndicesClosedByCommand(
      int index,
      ContextMenuCommand id) const = 0;

  // Returns true if 'CommandToggleTabAudioMuted' will mute. |index| is the
  // index supplied to |ExecuteContextMenuCommand|.
  virtual bool WillContextMenuMute(int index) = 0;

  // Returns true if 'CommandToggleSiteMuted' will mute. |index| is the
  // index supplied to |ExecuteContextMenuCommand|.
  virtual bool WillContextMenuMuteSites(int index) = 0;

  // Returns true if 'CommandTogglePinned' will pin. |index| is the index
  // supplied to |ExecuteContextMenuCommand|.
  virtual bool WillContextMenuPin(int index) = 0;

  // Convert a ContextMenuCommand into a browser command. Returns true if a
  // corresponding browser command exists, false otherwise.
  static bool ContextMenuCommandToBrowserCommand(int cmd_id, int* browser_cmd);

 protected:
  base::ObserverList<TabStripModelObserver>& observers() { return observers_; }

  // WebContentsCloseDelegate:
  bool ContainsWebContents(content::WebContents* contents) override;
  void OnWillDeleteWebContents(content::WebContents* contents,
                               uint32_t close_types) override;
  bool RunUnloadListenerBeforeClosing(content::WebContents* contents) override;
  bool ShouldRunUnloadListenerBeforeClosing(
      content::WebContents* contents) override;

 private:
  TabStripModelDelegate* delegate_;
  base::ObserverList<TabStripModelObserver> observers_;
};

#endif  // CHROME_BROWSER_UI_TABS_TAB_STRIP_MODEL_H_
