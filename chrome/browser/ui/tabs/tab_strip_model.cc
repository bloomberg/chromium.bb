// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model.h"

#include <algorithm>
#include <map>
#include <string>

#include "base/metrics/histogram.h"
#include "base/stl_util.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_order_controller.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"

using content::UserMetricsAction;
using content::WebContents;

namespace {

// Returns true if the specified transition is one of the types that cause the
// opener relationships for the tab in which the transition occurred to be
// forgotten. This is generally any navigation that isn't a link click (i.e.
// any navigation that can be considered to be the start of a new task distinct
// from what had previously occurred in that tab).
bool ShouldForgetOpenersForTransition(content::PageTransition transition) {
  return transition == content::PAGE_TRANSITION_TYPED ||
      transition == content::PAGE_TRANSITION_AUTO_BOOKMARK ||
      transition == content::PAGE_TRANSITION_GENERATED ||
      transition == content::PAGE_TRANSITION_KEYWORD ||
      transition == content::PAGE_TRANSITION_AUTO_TOPLEVEL;
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// TabStripModel, public:

TabStripModel::TabStripModel(TabStripModelDelegate* delegate, Profile* profile)
    : delegate_(delegate),
      profile_(profile),
      closing_all_(false) {
  DCHECK(delegate_);
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::NotificationService::AllBrowserContextsAndSources());
  order_controller_.reset(new TabStripModelOrderController(this));
}

TabStripModel::~TabStripModel() {
  FOR_EACH_OBSERVER(TabStripModelObserver, observers_,
                    TabStripModelDeleted());
  STLDeleteElements(&contents_data_);
  order_controller_.reset();
}

void TabStripModel::AddObserver(TabStripModelObserver* observer) {
  observers_.AddObserver(observer);
}

void TabStripModel::RemoveObserver(TabStripModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

bool TabStripModel::ContainsIndex(int index) const {
  return index >= 0 && index < count();
}

void TabStripModel::AppendWebContents(WebContents* contents,
                                      bool foreground) {
  InsertWebContentsAt(count(), contents,
                      foreground ? (ADD_INHERIT_GROUP | ADD_ACTIVE) :
                                   ADD_NONE);
}

void TabStripModel::InsertWebContentsAt(int index,
                                        WebContents* contents,
                                        int add_types) {
  delegate_->WillAddWebContents(contents);

  bool active = add_types & ADD_ACTIVE;
  // Force app tabs to be pinned.
  extensions::TabHelper* extensions_tab_helper =
      extensions::TabHelper::FromWebContents(contents);
  bool pin = extensions_tab_helper->is_app() || add_types & ADD_PINNED;
  index = ConstrainInsertionIndex(index, pin);

  // In tab dragging situations, if the last tab in the window was detached
  // then the user aborted the drag, we will have the |closing_all_| member
  // set (see DetachWebContentsAt) which will mess with our mojo here. We need
  // to clear this bit.
  closing_all_ = false;

  // Have to get the active contents before we monkey with |contents_|
  // otherwise we run into problems when we try to change the active contents
  // since the old contents and the new contents will be the same...
  WebContents* active_contents = GetActiveWebContents();
  WebContentsData* data = new WebContentsData(contents);
  data->pinned = pin;
  if ((add_types & ADD_INHERIT_GROUP) && active_contents) {
    if (active) {
      // Forget any existing relationships, we don't want to make things too
      // confusing by having multiple groups active at the same time.
      ForgetAllOpeners();
    }
    // Anything opened by a link we deem to have an opener.
    data->SetGroup(active_contents);
  } else if ((add_types & ADD_INHERIT_OPENER) && active_contents) {
    if (active) {
      // Forget any existing relationships, we don't want to make things too
      // confusing by having multiple groups active at the same time.
      ForgetAllOpeners();
    }
    data->opener = active_contents;
  }

  contents_data_.insert(contents_data_.begin() + index, data);

  selection_model_.IncrementFrom(index);

  FOR_EACH_OBSERVER(TabStripModelObserver, observers_,
                    TabInsertedAt(contents, index, active));
  if (active) {
    ui::ListSelectionModel new_model;
    new_model.Copy(selection_model_);
    new_model.SetSelectedIndex(index);
    SetSelection(new_model, NOTIFY_DEFAULT);
  }
}

WebContents* TabStripModel::ReplaceWebContentsAt(int index,
                                                 WebContents* new_contents) {
  delegate_->WillAddWebContents(new_contents);

  DCHECK(ContainsIndex(index));
  WebContents* old_contents = GetWebContentsAtImpl(index);

  ForgetOpenersAndGroupsReferencing(old_contents);

  contents_data_[index]->contents = new_contents;

  FOR_EACH_OBSERVER(TabStripModelObserver, observers_,
                    TabReplacedAt(this, old_contents, new_contents, index));

  // When the active WebContents is replaced send out a selection notification
  // too. We do this as nearly all observers need to treat a replacement of the
  // selected contents as the selection changing.
  if (active_index() == index) {
    FOR_EACH_OBSERVER(TabStripModelObserver, observers_,
                      ActiveTabChanged(old_contents, new_contents,
                                       active_index(), false));
  }
  return old_contents;
}

WebContents* TabStripModel::DiscardWebContentsAt(int index) {
  DCHECK(ContainsIndex(index));
  // Do not discard active tab.
  if (active_index() == index)
    return NULL;

  WebContents* null_contents =
      WebContents::Create(WebContents::CreateParams(profile()));
  WebContents* old_contents = GetWebContentsAtImpl(index);
  // Copy over the state from the navigation controller so we preserve the
  // back/forward history and continue to display the correct title/favicon.
  null_contents->GetController().CopyStateFrom(old_contents->GetController());
  // Replace the tab we're discarding with the null version.
  ReplaceWebContentsAt(index, null_contents);
  // Mark the tab so it will reload when we click.
  contents_data_[index]->discarded = true;
  // Discard the old tab's renderer.
  // TODO(jamescook): This breaks script connections with other tabs.
  // We need to find a different approach that doesn't do that, perhaps based
  // on navigation to swappedout://.
  delete old_contents;
  return null_contents;
}

WebContents* TabStripModel::DetachWebContentsAt(int index) {
  if (contents_data_.empty())
    return NULL;

  DCHECK(ContainsIndex(index));

  WebContents* removed_contents = GetWebContentsAtImpl(index);
  bool was_selected = IsTabSelected(index);
  int next_selected_index = order_controller_->DetermineNewSelectedIndex(index);
  delete contents_data_[index];
  contents_data_.erase(contents_data_.begin() + index);
  ForgetOpenersAndGroupsReferencing(removed_contents);
  if (empty())
    closing_all_ = true;
  FOR_EACH_OBSERVER(TabStripModelObserver, observers_,
                    TabDetachedAt(removed_contents, index));
  if (empty()) {
    selection_model_.Clear();
    // TabDetachedAt() might unregister observers, so send |TabStripEmpty()| in
    // a second pass.
    FOR_EACH_OBSERVER(TabStripModelObserver, observers_, TabStripEmpty());
  } else {
    int old_active = active_index();
    selection_model_.DecrementFrom(index);
    ui::ListSelectionModel old_model;
    old_model.Copy(selection_model_);
    if (index == old_active) {
      NotifyIfTabDeactivated(removed_contents);
      if (!selection_model_.empty()) {
        // The active tab was removed, but there is still something selected.
        // Move the active and anchor to the first selected index.
        selection_model_.set_active(selection_model_.selected_indices()[0]);
        selection_model_.set_anchor(selection_model_.active());
      } else {
        // The active tab was removed and nothing is selected. Reset the
        // selection and send out notification.
        selection_model_.SetSelectedIndex(next_selected_index);
      }
      NotifyIfActiveTabChanged(removed_contents, NOTIFY_DEFAULT);
    }

    // Sending notification in case the detached tab was selected. Using
    // NotifyIfActiveOrSelectionChanged() here would not guarantee that a
    // notification is sent even though the tab selection has changed because
    // |old_model| is stored after calling DecrementFrom().
    if (was_selected) {
      FOR_EACH_OBSERVER(TabStripModelObserver, observers_,
                        TabSelectionChanged(this, old_model));
    }
  }
  return removed_contents;
}

void TabStripModel::ActivateTabAt(int index, bool user_gesture) {
  DCHECK(ContainsIndex(index));
  ui::ListSelectionModel new_model;
  new_model.Copy(selection_model_);
  new_model.SetSelectedIndex(index);
  SetSelection(new_model, user_gesture ? NOTIFY_USER_GESTURE : NOTIFY_DEFAULT);
}

void TabStripModel::AddTabAtToSelection(int index) {
  DCHECK(ContainsIndex(index));
  ui::ListSelectionModel new_model;
  new_model.Copy(selection_model_);
  new_model.AddIndexToSelection(index);
  SetSelection(new_model, NOTIFY_DEFAULT);
}

void TabStripModel::MoveWebContentsAt(int index,
                                      int to_position,
                                      bool select_after_move) {
  DCHECK(ContainsIndex(index));
  if (index == to_position)
    return;

  int first_non_mini_tab = IndexOfFirstNonMiniTab();
  if ((index < first_non_mini_tab && to_position >= first_non_mini_tab) ||
      (to_position < first_non_mini_tab && index >= first_non_mini_tab)) {
    // This would result in mini tabs mixed with non-mini tabs. We don't allow
    // that.
    return;
  }

  MoveWebContentsAtImpl(index, to_position, select_after_move);
}

void TabStripModel::MoveSelectedTabsTo(int index) {
  int total_mini_count = IndexOfFirstNonMiniTab();
  int selected_mini_count = 0;
  int selected_count =
      static_cast<int>(selection_model_.selected_indices().size());
  for (int i = 0; i < selected_count &&
           IsMiniTab(selection_model_.selected_indices()[i]); ++i) {
    selected_mini_count++;
  }

  // To maintain that all mini-tabs occur before non-mini-tabs we move them
  // first.
  if (selected_mini_count > 0) {
    MoveSelectedTabsToImpl(
        std::min(total_mini_count - selected_mini_count, index), 0u,
        selected_mini_count);
    if (index > total_mini_count - selected_mini_count) {
      // We're being told to drag mini-tabs to an invalid location. Adjust the
      // index such that non-mini-tabs end up at a location as though we could
      // move the mini-tabs to index. See description in header for more
      // details.
      index += selected_mini_count;
    }
  }
  if (selected_mini_count == selected_count)
    return;

  // Then move the non-pinned tabs.
  MoveSelectedTabsToImpl(std::max(index, total_mini_count),
                         selected_mini_count,
                         selected_count - selected_mini_count);
}

WebContents* TabStripModel::GetActiveWebContents() const {
  return GetWebContentsAt(active_index());
}

WebContents* TabStripModel::GetWebContentsAt(int index) const {
  if (ContainsIndex(index))
    return GetWebContentsAtImpl(index);
  return NULL;
}

int TabStripModel::GetIndexOfWebContents(const WebContents* contents) const {
  for (size_t i = 0; i < contents_data_.size(); ++i) {
    if (contents_data_[i]->contents == contents)
      return i;
  }
  return kNoTab;
}

void TabStripModel::UpdateWebContentsStateAt(int index,
    TabStripModelObserver::TabChangeType change_type) {
  DCHECK(ContainsIndex(index));

  FOR_EACH_OBSERVER(TabStripModelObserver, observers_,
      TabChangedAt(GetWebContentsAtImpl(index), index, change_type));
}

void TabStripModel::CloseAllTabs() {
  // Set state so that observers can adjust their behavior to suit this
  // specific condition when CloseWebContentsAt causes a flurry of
  // Close/Detach/Select notifications to be sent.
  closing_all_ = true;
  std::vector<int> closing_tabs;
  for (int i = count() - 1; i >= 0; --i)
    closing_tabs.push_back(i);
  InternalCloseTabs(closing_tabs, CLOSE_CREATE_HISTORICAL_TAB);
}

bool TabStripModel::CloseWebContentsAt(int index, uint32 close_types) {
  DCHECK(ContainsIndex(index));
  std::vector<int> closing_tabs;
  closing_tabs.push_back(index);
  return InternalCloseTabs(closing_tabs, close_types);
}

bool TabStripModel::TabsAreLoading() const {
  for (WebContentsDataVector::const_iterator iter = contents_data_.begin();
       iter != contents_data_.end(); ++iter) {
    if ((*iter)->contents->IsLoading())
      return true;
  }
  return false;
}

WebContents* TabStripModel::GetOpenerOfWebContentsAt(int index) {
  DCHECK(ContainsIndex(index));
  return contents_data_[index]->opener;
}

void TabStripModel::SetOpenerOfWebContentsAt(int index,
                                             WebContents* opener) {
  DCHECK(ContainsIndex(index));
  DCHECK(opener);
  contents_data_[index]->opener = opener;
}

int TabStripModel::GetIndexOfNextWebContentsOpenedBy(const WebContents* opener,
                                                     int start_index,
                                                     bool use_group) const {
  DCHECK(opener);
  DCHECK(ContainsIndex(start_index));

  // Check tabs after start_index first.
  for (int i = start_index + 1; i < count(); ++i) {
    if (OpenerMatches(contents_data_[i], opener, use_group))
      return i;
  }
  // Then check tabs before start_index, iterating backwards.
  for (int i = start_index - 1; i >= 0; --i) {
    if (OpenerMatches(contents_data_[i], opener, use_group))
      return i;
  }
  return kNoTab;
}

int TabStripModel::GetIndexOfLastWebContentsOpenedBy(const WebContents* opener,
                                                     int start_index) const {
  DCHECK(opener);
  DCHECK(ContainsIndex(start_index));

  for (int i = contents_data_.size() - 1; i > start_index; --i) {
    if (contents_data_[i]->opener == opener)
      return i;
  }
  return kNoTab;
}

void TabStripModel::TabNavigating(WebContents* contents,
                                  content::PageTransition transition) {
  if (ShouldForgetOpenersForTransition(transition)) {
    // Don't forget the openers if this tab is a New Tab page opened at the
    // end of the TabStrip (e.g. by pressing Ctrl+T). Give the user one
    // navigation of one of these transition types before resetting the
    // opener relationships (this allows for the use case of opening a new
    // tab to do a quick look-up of something while viewing a tab earlier in
    // the strip). We can make this heuristic more permissive if need be.
    if (!IsNewTabAtEndOfTabStrip(contents)) {
      // If the user navigates the current tab to another page in any way
      // other than by clicking a link, we want to pro-actively forget all
      // TabStrip opener relationships since we assume they're beginning a
      // different task by reusing the current tab.
      ForgetAllOpeners();
      // In this specific case we also want to reset the group relationship,
      // since it is now technically invalid.
      ForgetGroup(contents);
    }
  }
}

void TabStripModel::ForgetAllOpeners() {
  // Forget all opener memories so we don't do anything weird with tab
  // re-selection ordering.
  for (WebContentsDataVector::const_iterator iter = contents_data_.begin();
       iter != contents_data_.end(); ++iter)
    (*iter)->ForgetOpener();
}

void TabStripModel::ForgetGroup(WebContents* contents) {
  int index = GetIndexOfWebContents(contents);
  DCHECK(ContainsIndex(index));
  contents_data_[index]->SetGroup(NULL);
  contents_data_[index]->ForgetOpener();
}

bool TabStripModel::ShouldResetGroupOnSelect(WebContents* contents) const {
  int index = GetIndexOfWebContents(contents);
  DCHECK(ContainsIndex(index));
  return contents_data_[index]->reset_group_on_select;
}

void TabStripModel::SetTabBlocked(int index, bool blocked) {
  DCHECK(ContainsIndex(index));
  if (contents_data_[index]->blocked == blocked)
    return;
  contents_data_[index]->blocked = blocked;
  FOR_EACH_OBSERVER(TabStripModelObserver, observers_,
                    TabBlockedStateChanged(contents_data_[index]->contents,
                                           index));
}

void TabStripModel::SetTabPinned(int index, bool pinned) {
  DCHECK(ContainsIndex(index));
  if (contents_data_[index]->pinned == pinned)
    return;

  if (IsAppTab(index)) {
    if (!pinned) {
      // App tabs should always be pinned.
      NOTREACHED();
      return;
    }
    // Changing the pinned state of an app tab doesn't affect its mini-tab
    // status.
    contents_data_[index]->pinned = pinned;
  } else {
    // The tab is not an app tab, its position may have to change as the
    // mini-tab state is changing.
    int non_mini_tab_index = IndexOfFirstNonMiniTab();
    contents_data_[index]->pinned = pinned;
    if (pinned && index != non_mini_tab_index) {
      MoveWebContentsAtImpl(index, non_mini_tab_index, false);
      index = non_mini_tab_index;
    } else if (!pinned && index + 1 != non_mini_tab_index) {
      MoveWebContentsAtImpl(index, non_mini_tab_index - 1, false);
      index = non_mini_tab_index - 1;
    }

    FOR_EACH_OBSERVER(TabStripModelObserver, observers_,
                      TabMiniStateChanged(contents_data_[index]->contents,
                                          index));
  }

  // else: the tab was at the boundary and its position doesn't need to change.
  FOR_EACH_OBSERVER(TabStripModelObserver, observers_,
                    TabPinnedStateChanged(contents_data_[index]->contents,
                                          index));
}

bool TabStripModel::IsTabPinned(int index) const {
  DCHECK(ContainsIndex(index));
  return contents_data_[index]->pinned;
}

bool TabStripModel::IsMiniTab(int index) const {
  return IsTabPinned(index) || IsAppTab(index);
}

bool TabStripModel::IsAppTab(int index) const {
  WebContents* contents = GetWebContentsAt(index);
  return contents && extensions::TabHelper::FromWebContents(contents)->is_app();
}

bool TabStripModel::IsTabBlocked(int index) const {
  return contents_data_[index]->blocked;
}

bool TabStripModel::IsTabDiscarded(int index) const {
  return contents_data_[index]->discarded;
}

int TabStripModel::IndexOfFirstNonMiniTab() const {
  for (size_t i = 0; i < contents_data_.size(); ++i) {
    if (!IsMiniTab(static_cast<int>(i)))
      return static_cast<int>(i);
  }
  // No mini-tabs.
  return count();
}

int TabStripModel::ConstrainInsertionIndex(int index, bool mini_tab) {
  return mini_tab ? std::min(std::max(0, index), IndexOfFirstNonMiniTab()) :
      std::min(count(), std::max(index, IndexOfFirstNonMiniTab()));
}

void TabStripModel::ExtendSelectionTo(int index) {
  DCHECK(ContainsIndex(index));
  ui::ListSelectionModel new_model;
  new_model.Copy(selection_model_);
  new_model.SetSelectionFromAnchorTo(index);
  SetSelection(new_model, NOTIFY_DEFAULT);
}

void TabStripModel::ToggleSelectionAt(int index) {
  DCHECK(ContainsIndex(index));
  ui::ListSelectionModel new_model;
  new_model.Copy(selection_model());
  if (selection_model_.IsSelected(index)) {
    if (selection_model_.size() == 1) {
      // One tab must be selected and this tab is currently selected so we can't
      // unselect it.
      return;
    }
    new_model.RemoveIndexFromSelection(index);
    new_model.set_anchor(index);
    if (new_model.active() == index ||
        new_model.active() == ui::ListSelectionModel::kUnselectedIndex)
      new_model.set_active(new_model.selected_indices()[0]);
  } else {
    new_model.AddIndexToSelection(index);
    new_model.set_anchor(index);
    new_model.set_active(index);
  }
  SetSelection(new_model, NOTIFY_DEFAULT);
}

void TabStripModel::AddSelectionFromAnchorTo(int index) {
  ui::ListSelectionModel new_model;
  new_model.Copy(selection_model_);
  new_model.AddSelectionFromAnchorTo(index);
  SetSelection(new_model, NOTIFY_DEFAULT);
}

bool TabStripModel::IsTabSelected(int index) const {
  DCHECK(ContainsIndex(index));
  return selection_model_.IsSelected(index);
}

void TabStripModel::SetSelectionFromModel(
    const ui::ListSelectionModel& source) {
  DCHECK_NE(ui::ListSelectionModel::kUnselectedIndex, source.active());
  SetSelection(source, NOTIFY_DEFAULT);
}

void TabStripModel::AddWebContents(WebContents* contents,
                                   int index,
                                   content::PageTransition transition,
                                   int add_types) {
  // If the newly-opened tab is part of the same task as the parent tab, we want
  // to inherit the parent's "group" attribute, so that if this tab is then
  // closed we'll jump back to the parent tab.
  bool inherit_group = (add_types & ADD_INHERIT_GROUP) == ADD_INHERIT_GROUP;

  if (transition == content::PAGE_TRANSITION_LINK &&
      (add_types & ADD_FORCE_INDEX) == 0) {
    // We assume tabs opened via link clicks are part of the same task as their
    // parent.  Note that when |force_index| is true (e.g. when the user
    // drag-and-drops a link to the tab strip), callers aren't really handling
    // link clicks, they just want to score the navigation like a link click in
    // the history backend, so we don't inherit the group in this case.
    index = order_controller_->DetermineInsertionIndex(transition,
                                                       add_types & ADD_ACTIVE);
    inherit_group = true;
  } else {
    // For all other types, respect what was passed to us, normalizing -1s and
    // values that are too large.
    if (index < 0 || index > count())
      index = count();
  }

  if (transition == content::PAGE_TRANSITION_TYPED && index == count()) {
    // Also, any tab opened at the end of the TabStrip with a "TYPED"
    // transition inherit group as well. This covers the cases where the user
    // creates a New Tab (e.g. Ctrl+T, or clicks the New Tab button), or types
    // in the address bar and presses Alt+Enter. This allows for opening a new
    // Tab to quickly look up something. When this Tab is closed, the old one
    // is re-selected, not the next-adjacent.
    inherit_group = true;
  }
  InsertWebContentsAt(index, contents,
                      add_types | (inherit_group ? ADD_INHERIT_GROUP : 0));
  // Reset the index, just in case insert ended up moving it on us.
  index = GetIndexOfWebContents(contents);

  if (inherit_group && transition == content::PAGE_TRANSITION_TYPED)
    contents_data_[index]->reset_group_on_select = true;

  // TODO(sky): figure out why this is here and not in InsertWebContentsAt. When
  // here we seem to get failures in startup perf tests.
  // Ensure that the new WebContentsView begins at the same size as the
  // previous WebContentsView if it existed.  Otherwise, the initial WebKit
  // layout will be performed based on a width of 0 pixels, causing a
  // very long, narrow, inaccurate layout.  Because some scripts on pages (as
  // well as WebKit's anchor link location calculation) are run on the
  // initial layout and not recalculated later, we need to ensure the first
  // layout is performed with sane view dimensions even when we're opening a
  // new background tab.
  if (WebContents* old_contents = GetActiveWebContents()) {
    if ((add_types & ADD_ACTIVE) == 0) {
      contents->GetView()->SizeContents(
          old_contents->GetView()->GetContainerSize());
      // We need to hide the contents or else we get and execute paints for
      // background tabs. With enough background tabs they will steal the
      // backing store of the visible tab causing flashing. See bug 20831.
      contents->WasHidden();
    }
  }
}

void TabStripModel::CloseSelectedTabs() {
  InternalCloseTabs(selection_model_.selected_indices(),
                    CLOSE_CREATE_HISTORICAL_TAB | CLOSE_USER_GESTURE);
}

void TabStripModel::SelectNextTab() {
  SelectRelativeTab(true);
}

void TabStripModel::SelectPreviousTab() {
  SelectRelativeTab(false);
}

void TabStripModel::SelectLastTab() {
  ActivateTabAt(count() - 1, true);
}

void TabStripModel::MoveTabNext() {
  // TODO: this likely needs to be updated for multi-selection.
  int new_index = std::min(active_index() + 1, count() - 1);
  MoveWebContentsAt(active_index(), new_index, true);
}

void TabStripModel::MoveTabPrevious() {
  // TODO: this likely needs to be updated for multi-selection.
  int new_index = std::max(active_index() - 1, 0);
  MoveWebContentsAt(active_index(), new_index, true);
}

// Context menu functions.
bool TabStripModel::IsContextMenuCommandEnabled(
    int context_index, ContextMenuCommand command_id) const {
  DCHECK(command_id > CommandFirst && command_id < CommandLast);
  switch (command_id) {
    case CommandNewTab:
    case CommandCloseTab:
      return true;

    case CommandReload: {
      std::vector<int> indices = GetIndicesForCommand(context_index);
      for (size_t i = 0; i < indices.size(); ++i) {
        WebContents* tab = GetWebContentsAt(indices[i]);
        if (tab) {
          CoreTabHelperDelegate* core_delegate =
              CoreTabHelper::FromWebContents(tab)->delegate();
          if (!core_delegate || core_delegate->CanReloadContents(tab))
            return true;
        }
      }
      return false;
    }

    case CommandCloseOtherTabs:
    case CommandCloseTabsToRight:
      return !GetIndicesClosedByCommand(context_index, command_id).empty();

    case CommandDuplicate: {
      std::vector<int> indices = GetIndicesForCommand(context_index);
      for (size_t i = 0; i < indices.size(); ++i) {
        if (delegate_->CanDuplicateContentsAt(indices[i]))
          return true;
      }
      return false;
    }

    case CommandRestoreTab:
      return delegate_->GetRestoreTabType() !=
          TabStripModelDelegate::RESTORE_NONE;

    case CommandTogglePinned: {
      std::vector<int> indices = GetIndicesForCommand(context_index);
      for (size_t i = 0; i < indices.size(); ++i) {
        if (!IsAppTab(indices[i]))
          return true;
      }
      return false;
    }

    case CommandBookmarkAllTabs:
      return browser_defaults::bookmarks_enabled &&
          delegate_->CanBookmarkAllTabs();

    case CommandSelectByDomain:
    case CommandSelectByOpener:
      return true;

    default:
      NOTREACHED();
  }
  return false;
}

void TabStripModel::ExecuteContextMenuCommand(
    int context_index, ContextMenuCommand command_id) {
  DCHECK(command_id > CommandFirst && command_id < CommandLast);
  switch (command_id) {
    case CommandNewTab:
      content::RecordAction(UserMetricsAction("TabContextMenu_NewTab"));
      UMA_HISTOGRAM_ENUMERATION("Tab.NewTab",
                                TabStripModel::NEW_TAB_CONTEXT_MENU,
                                TabStripModel::NEW_TAB_ENUM_COUNT);
      delegate()->AddBlankTabAt(context_index + 1, true);
      break;

    case CommandReload: {
      content::RecordAction(UserMetricsAction("TabContextMenu_Reload"));
      std::vector<int> indices = GetIndicesForCommand(context_index);
      for (size_t i = 0; i < indices.size(); ++i) {
        WebContents* tab = GetWebContentsAt(indices[i]);
        if (tab) {
          CoreTabHelperDelegate* core_delegate =
              CoreTabHelper::FromWebContents(tab)->delegate();
          if (!core_delegate || core_delegate->CanReloadContents(tab))
            tab->GetController().Reload(true);
        }
      }
      break;
    }

    case CommandDuplicate: {
      content::RecordAction(UserMetricsAction("TabContextMenu_Duplicate"));
      std::vector<int> indices = GetIndicesForCommand(context_index);
      // Copy the WebContents off as the indices will change as tabs are
      // duplicated.
      std::vector<WebContents*> tabs;
      for (size_t i = 0; i < indices.size(); ++i)
        tabs.push_back(GetWebContentsAt(indices[i]));
      for (size_t i = 0; i < tabs.size(); ++i) {
        int index = GetIndexOfWebContents(tabs[i]);
        if (index != -1 && delegate_->CanDuplicateContentsAt(index))
          delegate_->DuplicateContentsAt(index);
      }
      break;
    }

    case CommandCloseTab: {
      content::RecordAction(UserMetricsAction("TabContextMenu_CloseTab"));
      InternalCloseTabs(GetIndicesForCommand(context_index),
                        CLOSE_CREATE_HISTORICAL_TAB | CLOSE_USER_GESTURE);
      break;
    }

    case CommandCloseOtherTabs: {
      content::RecordAction(
          UserMetricsAction("TabContextMenu_CloseOtherTabs"));
      InternalCloseTabs(GetIndicesClosedByCommand(context_index, command_id),
                        CLOSE_CREATE_HISTORICAL_TAB);
      break;
    }

    case CommandCloseTabsToRight: {
      content::RecordAction(
          UserMetricsAction("TabContextMenu_CloseTabsToRight"));
      InternalCloseTabs(GetIndicesClosedByCommand(context_index, command_id),
                        CLOSE_CREATE_HISTORICAL_TAB);
      break;
    }

    case CommandRestoreTab: {
      content::RecordAction(UserMetricsAction("TabContextMenu_RestoreTab"));
      delegate_->RestoreTab();
      break;
    }

    case CommandTogglePinned: {
      content::RecordAction(
          UserMetricsAction("TabContextMenu_TogglePinned"));
      std::vector<int> indices = GetIndicesForCommand(context_index);
      bool pin = WillContextMenuPin(context_index);
      if (pin) {
        for (size_t i = 0; i < indices.size(); ++i) {
          if (!IsAppTab(indices[i]))
            SetTabPinned(indices[i], true);
        }
      } else {
        // Unpin from the back so that the order is maintained (unpinning can
        // trigger moving a tab).
        for (size_t i = indices.size(); i > 0; --i) {
          if (!IsAppTab(indices[i - 1]))
            SetTabPinned(indices[i - 1], false);
        }
      }
      break;
    }

    case CommandBookmarkAllTabs: {
      content::RecordAction(
          UserMetricsAction("TabContextMenu_BookmarkAllTabs"));

      delegate_->BookmarkAllTabs();
      break;
    }

    case CommandSelectByDomain:
    case CommandSelectByOpener: {
      std::vector<int> indices;
      if (command_id == CommandSelectByDomain)
        GetIndicesWithSameDomain(context_index, &indices);
      else
        GetIndicesWithSameOpener(context_index, &indices);
      ui::ListSelectionModel selection_model;
      selection_model.SetSelectedIndex(context_index);
      for (size_t i = 0; i < indices.size(); ++i)
        selection_model.AddIndexToSelection(indices[i]);
      SetSelectionFromModel(selection_model);
      break;
    }

    default:
      NOTREACHED();
  }
}

std::vector<int> TabStripModel::GetIndicesClosedByCommand(
    int index,
    ContextMenuCommand id) const {
  DCHECK(ContainsIndex(index));
  DCHECK(id == CommandCloseTabsToRight || id == CommandCloseOtherTabs);
  bool is_selected = IsTabSelected(index);
  int start;
  if (id == CommandCloseTabsToRight) {
    if (is_selected) {
      start = selection_model_.selected_indices()[
          selection_model_.selected_indices().size() - 1] + 1;
    } else {
      start = index + 1;
    }
  } else {
    start = 0;
  }
  // NOTE: callers expect the vector to be sorted in descending order.
  std::vector<int> indices;
  for (int i = count() - 1; i >= start; --i) {
    if (i != index && !IsMiniTab(i) && (!is_selected || !IsTabSelected(i)))
      indices.push_back(i);
  }
  return indices;
}

bool TabStripModel::WillContextMenuPin(int index) {
  std::vector<int> indices = GetIndicesForCommand(index);
  // If all tabs are pinned, then we unpin, otherwise we pin.
  bool all_pinned = true;
  for (size_t i = 0; i < indices.size() && all_pinned; ++i) {
    if (!IsAppTab(index))  // We never change app tabs.
      all_pinned = IsTabPinned(indices[i]);
  }
  return !all_pinned;
}

///////////////////////////////////////////////////////////////////////////////
// TabStripModel, content::NotificationObserver implementation:

void TabStripModel::Observe(int type,
                            const content::NotificationSource& source,
                            const content::NotificationDetails& details) {
  switch (type) {
    case content::NOTIFICATION_WEB_CONTENTS_DESTROYED: {
      // Sometimes, on qemu, it seems like a WebContents object can be destroyed
      // while we still have a reference to it. We need to break this reference
      // here so we don't crash later.
      int index = GetIndexOfWebContents(
          content::Source<WebContents>(source).ptr());
      if (index != TabStripModel::kNoTab) {
        // Note that we only detach the contents here, not close it - it's
        // already been closed. We just want to undo our bookkeeping.
        DetachWebContentsAt(index);
      }
      break;
    }

    default:
      NOTREACHED();
  }
}

// static
bool TabStripModel::ContextMenuCommandToBrowserCommand(int cmd_id,
                                                       int* browser_cmd) {
  switch (cmd_id) {
    case CommandNewTab:
      *browser_cmd = IDC_NEW_TAB;
      break;
    case CommandReload:
      *browser_cmd = IDC_RELOAD;
      break;
    case CommandDuplicate:
      *browser_cmd = IDC_DUPLICATE_TAB;
      break;
    case CommandCloseTab:
      *browser_cmd = IDC_CLOSE_TAB;
      break;
    case CommandRestoreTab:
      *browser_cmd = IDC_RESTORE_TAB;
      break;
    case CommandBookmarkAllTabs:
      *browser_cmd = IDC_BOOKMARK_ALL_TABS;
      break;
    default:
      *browser_cmd = 0;
      return false;
  }

  return true;
}

///////////////////////////////////////////////////////////////////////////////
// TabStripModel, private:

void TabStripModel::GetIndicesWithSameDomain(int index,
                                             std::vector<int>* indices) {
  std::string domain = GetWebContentsAt(index)->GetURL().host();
  if (domain.empty())
    return;
  for (int i = 0; i < count(); ++i) {
    if (i == index)
      continue;
    if (GetWebContentsAt(i)->GetURL().host() == domain)
      indices->push_back(i);
  }
}

void TabStripModel::GetIndicesWithSameOpener(int index,
                                             std::vector<int>* indices) {
  WebContents* opener = contents_data_[index]->group;
  if (!opener) {
    // If there is no group, find all tabs with the selected tab as the opener.
    opener = GetWebContentsAt(index);
    if (!opener)
      return;
  }
  for (int i = 0; i < count(); ++i) {
    if (i == index)
      continue;
    if (contents_data_[i]->group == opener ||
        GetWebContentsAtImpl(i) == opener) {
      indices->push_back(i);
    }
  }
}

std::vector<int> TabStripModel::GetIndicesForCommand(int index) const {
  if (!IsTabSelected(index)) {
    std::vector<int> indices;
    indices.push_back(index);
    return indices;
  }
  return selection_model_.selected_indices();
}

bool TabStripModel::IsNewTabAtEndOfTabStrip(WebContents* contents) const {
  const GURL& url = contents->GetURL();
  return url.SchemeIs(chrome::kChromeUIScheme) &&
         url.host() == chrome::kChromeUINewTabHost &&
         contents == GetWebContentsAtImpl(count() - 1) &&
         contents->GetController().GetEntryCount() == 1;
}

bool TabStripModel::InternalCloseTabs(const std::vector<int>& indices,
                                      uint32 close_types) {
  if (indices.empty())
    return true;

  // Map the indices to WebContentses, that way if deleting a tab deletes
  // other tabs we're ok. Crashes seem to indicate during tab deletion other
  // tabs are getting removed.
  std::vector<WebContents*> closing_contentses;
  for (size_t i = 0; i < indices.size(); ++i)
    closing_contentses.push_back(GetWebContentsAtImpl(indices[i]));

  // We only try the fast shutdown path if the whole browser process is *not*
  // shutting down. Fast shutdown during browser termination is handled in
  // BrowserShutdown.
  if (browser_shutdown::GetShutdownType() == browser_shutdown::NOT_VALID) {
    // Construct a map of processes to the number of associated tabs that are
    // closing.
    std::map<content::RenderProcessHost*, size_t> processes;
    for (size_t i = 0; i < indices.size(); ++i) {
      WebContents* closing_contents = GetWebContentsAtImpl(indices[i]);
      content::RenderProcessHost* process =
          closing_contents->GetRenderProcessHost();
      ++processes[process];
    }

    // Try to fast shutdown the tabs that can close.
    for (std::map<content::RenderProcessHost*, size_t>::iterator iter =
         processes.begin(); iter != processes.end(); ++iter) {
      iter->first->FastShutdownForPageCount(iter->second);
    }
  }

  // We now return to our regularly scheduled shutdown procedure.
  bool retval = true;
  for (size_t i = 0; i < closing_contentses.size(); ++i) {
    WebContents* closing_contents = closing_contentses[i];
    int index = GetIndexOfWebContents(closing_contents);
    // Make sure we still contain the tab.
    if (index == kNoTab)
      continue;

    closing_contents->OnCloseStarted();

    // Update the explicitly closed state. If the unload handlers cancel the
    // close the state is reset in Browser. We don't update the explicitly
    // closed state if already marked as explicitly closed as unload handlers
    // call back to this if the close is allowed.
    if (!closing_contents->GetClosedByUserGesture()) {
      closing_contents->SetClosedByUserGesture(
          close_types & CLOSE_USER_GESTURE);
    }

    if (delegate_->RunUnloadListenerBeforeClosing(closing_contents)) {
      retval = false;
      continue;
    }

    InternalCloseTab(closing_contents, index,
                     (close_types & CLOSE_CREATE_HISTORICAL_TAB) != 0);
  }

  return retval;
}

void TabStripModel::InternalCloseTab(WebContents* contents,
                                     int index,
                                     bool create_historical_tabs) {
  FOR_EACH_OBSERVER(TabStripModelObserver, observers_,
                    TabClosingAt(this, contents, index));

  // Ask the delegate to save an entry for this tab in the historical tab
  // database if applicable.
  if (create_historical_tabs)
    delegate_->CreateHistoricalTab(contents);

  // Deleting the WebContents will call back to us via
  // NotificationObserver and detach it.
  delete contents;
}

WebContents* TabStripModel::GetWebContentsAtImpl(int index) const {
  CHECK(ContainsIndex(index)) <<
      "Failed to find: " << index << " in: " << count() << " entries.";
  return contents_data_[index]->contents;
}

void TabStripModel::NotifyIfTabDeactivated(WebContents* contents) {
  if (contents) {
    FOR_EACH_OBSERVER(TabStripModelObserver, observers_,
                      TabDeactivated(contents));
  }
}

void TabStripModel::NotifyIfActiveTabChanged(WebContents* old_contents,
                                             NotifyTypes notify_types) {
  WebContents* new_contents = GetWebContentsAtImpl(active_index());
  if (old_contents != new_contents) {
    FOR_EACH_OBSERVER(TabStripModelObserver, observers_,
                      ActiveTabChanged(old_contents, new_contents,
                                       active_index(),
                                       notify_types == NOTIFY_USER_GESTURE));
    // Activating a discarded tab reloads it, so it is no longer discarded.
    contents_data_[active_index()]->discarded = false;
  }
}

void TabStripModel::NotifyIfActiveOrSelectionChanged(
    WebContents* old_contents,
    NotifyTypes notify_types,
    const ui::ListSelectionModel& old_model) {
  NotifyIfActiveTabChanged(old_contents, notify_types);

  if (!selection_model().Equals(old_model)) {
    FOR_EACH_OBSERVER(TabStripModelObserver, observers_,
                      TabSelectionChanged(this, old_model));
  }
}

void TabStripModel::SetSelection(
    const ui::ListSelectionModel& new_model,
    NotifyTypes notify_types) {
  WebContents* old_contents = GetActiveWebContents();
  ui::ListSelectionModel old_model;
  old_model.Copy(selection_model_);
  if (new_model.active() != selection_model_.active())
    NotifyIfTabDeactivated(old_contents);
  selection_model_.Copy(new_model);
  NotifyIfActiveOrSelectionChanged(old_contents, notify_types, old_model);
}

void TabStripModel::SelectRelativeTab(bool next) {
  // This may happen during automated testing or if a user somehow buffers
  // many key accelerators.
  if (contents_data_.empty())
    return;

  int index = active_index();
  int delta = next ? 1 : -1;
  index = (index + count() + delta) % count();
  ActivateTabAt(index, true);
}

void TabStripModel::MoveWebContentsAtImpl(int index,
                                          int to_position,
                                          bool select_after_move) {
  WebContentsData* moved_data = contents_data_[index];
  contents_data_.erase(contents_data_.begin() + index);
  contents_data_.insert(contents_data_.begin() + to_position, moved_data);

  selection_model_.Move(index, to_position);
  if (!selection_model_.IsSelected(select_after_move) && select_after_move) {
    // TODO(sky): why doesn't this code notify observers?
    selection_model_.SetSelectedIndex(to_position);
  }

  FOR_EACH_OBSERVER(TabStripModelObserver, observers_,
                    TabMoved(moved_data->contents, index, to_position));
}

void TabStripModel::MoveSelectedTabsToImpl(int index,
                                           size_t start,
                                           size_t length) {
  DCHECK(start < selection_model_.selected_indices().size() &&
         start + length <= selection_model_.selected_indices().size());
  size_t end = start + length;
  int count_before_index = 0;
  for (size_t i = start; i < end &&
       selection_model_.selected_indices()[i] < index + count_before_index;
       ++i) {
    count_before_index++;
  }

  // First move those before index. Any tabs before index end up moving in the
  // selection model so we use start each time through.
  int target_index = index + count_before_index;
  size_t tab_index = start;
  while (tab_index < end &&
         selection_model_.selected_indices()[start] < index) {
    MoveWebContentsAt(selection_model_.selected_indices()[start],
                      target_index - 1, false);
    tab_index++;
  }

  // Then move those after the index. These don't result in reordering the
  // selection.
  while (tab_index < end) {
    if (selection_model_.selected_indices()[tab_index] != target_index) {
      MoveWebContentsAt(selection_model_.selected_indices()[tab_index],
                        target_index, false);
    }
    tab_index++;
    target_index++;
  }
}

// static
bool TabStripModel::OpenerMatches(const WebContentsData* data,
                                  const WebContents* opener,
                                  bool use_group) {
  return data->opener == opener || (use_group && data->group == opener);
}

void TabStripModel::ForgetOpenersAndGroupsReferencing(
    const WebContents* tab) {
  for (WebContentsDataVector::const_iterator i = contents_data_.begin();
       i != contents_data_.end(); ++i) {
    if ((*i)->group == tab)
      (*i)->group = NULL;
    if ((*i)->opener == tab)
      (*i)->opener = NULL;
  }
}
