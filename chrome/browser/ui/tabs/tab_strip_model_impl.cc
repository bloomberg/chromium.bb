// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_impl.h"

#include <algorithm>
#include <set>
#include <string>
#include <utility>

#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "build/build_config.h"
#include "chrome/app/chrome_command_ids.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/extensions/tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_order_controller.h"
#include "chrome/browser/ui/tabs/tab_utils.h"
#include "chrome/browser/ui/web_contents_sizer.h"
#include "chrome/common/url_constants.h"
#include "components/feature_engagement/features.h"
#include "components/web_modal/web_contents_modal_dialog_manager.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"

#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
#include "chrome/browser/feature_engagement/new_tab/new_tab_tracker.h"
#include "chrome/browser/feature_engagement/new_tab/new_tab_tracker_factory.h"
#endif

using base::UserMetricsAction;
using content::WebContents;

namespace {

// Returns true if the specified transition is one of the types that cause the
// opener relationships for the tab in which the transition occurred to be
// forgotten. This is generally any navigation that isn't a link click (i.e.
// any navigation that can be considered to be the start of a new task distinct
// from what had previously occurred in that tab).
bool ShouldForgetOpenersForTransition(ui::PageTransition transition) {
  return ui::PageTransitionCoreTypeIs(transition, ui::PAGE_TRANSITION_TYPED) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_AUTO_BOOKMARK) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_GENERATED) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_KEYWORD) ||
         ui::PageTransitionCoreTypeIs(transition,
                                      ui::PAGE_TRANSITION_AUTO_TOPLEVEL);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// WebContentsData

// An object to hold a reference to a WebContents that is in a tabstrip, as
// well as other various properties it has.
class TabStripModelImpl::WebContentsData : public content::WebContentsObserver {
 public:
  WebContentsData(TabStripModel* tab_strip_model, WebContents* a_contents);

  // Changes the WebContents that this WebContentsData tracks.
  void SetWebContents(WebContents* contents);
  WebContents* web_contents() { return contents_; }

  // Create a relationship between this WebContentsData and other
  // WebContentses. Used to identify which WebContents to select next after
  // one is closed.
  WebContents* group() const { return group_; }
  void set_group(WebContents* value) { group_ = value; }
  WebContents* opener() const { return opener_; }
  void set_opener(WebContents* value) { opener_ = value; }

  // Alters the properties of the WebContents.
  bool reset_group_on_select() const { return reset_group_on_select_; }
  void set_reset_group_on_select(bool value) { reset_group_on_select_ = value; }
  bool pinned() const { return pinned_; }
  void set_pinned(bool value) { pinned_ = value; }
  bool blocked() const { return blocked_; }
  void set_blocked(bool value) { blocked_ = value; }

 private:
  // Make sure that if someone deletes this WebContents out from under us, it
  // is properly removed from the tab strip.
  void WebContentsDestroyed() override;

  // The WebContents being tracked by this WebContentsData. The
  // WebContentsObserver does keep a reference, but when the WebContents is
  // deleted, the WebContentsObserver reference is NULLed and thus inaccessible.
  WebContents* contents_;

  // The TabStripModel containing this WebContents.
  TabStripModel* tab_strip_model_;

  // The group is used to model a set of tabs spawned from a single parent
  // tab. This value is preserved for a given tab as long as the tab remains
  // navigated to the link it was initially opened at or some navigation from
  // that page (i.e. if the user types or visits a bookmark or some other
  // navigation within that tab, the group relationship is lost). This
  // property can safely be used to implement features that depend on a
  // logical group of related tabs.
  WebContents* group_ = nullptr;

  // The owner models the same relationship as group, except it is more
  // easily discarded, e.g. when the user switches to a tab not part of the
  // same group. This property is used to determine what tab to select next
  // when one is closed.
  WebContents* opener_ = nullptr;

  // True if our group should be reset the moment selection moves away from
  // this tab. This is the case for tabs opened in the foreground at the end
  // of the TabStrip while viewing another Tab. If these tabs are closed
  // before selection moves elsewhere, their opener is selected. But if
  // selection shifts to _any_ tab (including their opener), the group
  // relationship is reset to avoid confusing close sequencing.
  bool reset_group_on_select_ = false;

  // Is the tab pinned?
  bool pinned_ = false;

  // Is the tab interaction blocked by a modal dialog?
  bool blocked_ = false;

  DISALLOW_COPY_AND_ASSIGN(WebContentsData);
};

TabStripModelImpl::WebContentsData::WebContentsData(
    TabStripModel* tab_strip_model,
    WebContents* contents)
    : content::WebContentsObserver(contents),
      contents_(contents),
      tab_strip_model_(tab_strip_model) {}

void TabStripModelImpl::WebContentsData::SetWebContents(WebContents* contents) {
  contents_ = contents;
  Observe(contents);
}

void TabStripModelImpl::WebContentsData::WebContentsDestroyed() {
  DCHECK_EQ(contents_, web_contents());

  // Note that we only detach the contents here, not close it - it's
  // already been closed. We just want to undo our bookkeeping.
  int index = tab_strip_model_->GetIndexOfWebContents(web_contents());
  DCHECK_NE(TabStripModelImpl::kNoTab, index);
  tab_strip_model_->DetachWebContentsAt(index);
}

///////////////////////////////////////////////////////////////////////////////
// TabStripModel, public:

TabStripModelImpl::TabStripModelImpl(TabStripModelDelegate* delegate,
                                     Profile* profile)
    : TabStripModel(delegate), profile_(profile), weak_factory_(this) {
  order_controller_.reset(new TabStripModelOrderController(this));
}

TabStripModelImpl::~TabStripModelImpl() {
  contents_data_.clear();
  order_controller_.reset();
}

TabStripModelExperimental* TabStripModelImpl::AsTabStripModelExperimental() {
  return nullptr;
}

int TabStripModelImpl::count() const {
  return static_cast<int>(contents_data_.size());
}

bool TabStripModelImpl::empty() const {
  return contents_data_.empty();
}

Profile* TabStripModelImpl::profile() const {
  return profile_;
}

int TabStripModelImpl::active_index() const {
  return selection_model_.active();
}

bool TabStripModelImpl::closing_all() const {
  return closing_all_;
}

bool TabStripModelImpl::ContainsIndex(int index) const {
  return index >= 0 && index < count();
}

void TabStripModelImpl::AppendWebContents(WebContents* contents,
                                          bool foreground) {
  InsertWebContentsAt(count(), contents,
                      foreground ? (ADD_INHERIT_GROUP | ADD_ACTIVE) : ADD_NONE);
}

void TabStripModelImpl::InsertWebContentsAt(int index,
                                            WebContents* contents,
                                            int add_types) {
  delegate()->WillAddWebContents(contents);

  bool active = (add_types & ADD_ACTIVE) != 0;
  bool pin = (add_types & ADD_PINNED) != 0;
  index = ConstrainInsertionIndex(index, pin);

  // In tab dragging situations, if the last tab in the window was detached
  // then the user aborted the drag, we will have the |closing_all_| member
  // set (see DetachWebContentsAt) which will mess with our mojo here. We need
  // to clear this bit.
  closing_all_ = false;

  // Have to get the active contents before we monkey with the contents
  // otherwise we run into problems when we try to change the active contents
  // since the old contents and the new contents will be the same...
  WebContents* active_contents = GetActiveWebContents();
  std::unique_ptr<WebContentsData> data =
      base::MakeUnique<WebContentsData>(this, contents);
  data->set_pinned(pin);
  if ((add_types & ADD_INHERIT_GROUP) && active_contents) {
    if (active) {
      // Forget any existing relationships, we don't want to make things too
      // confusing by having multiple groups active at the same time.
      ForgetAllOpeners();
    }
    // Anything opened by a link we deem to have an opener.
    data->set_group(active_contents);
    data->set_opener(active_contents);
  } else if ((add_types & ADD_INHERIT_OPENER) && active_contents) {
    if (active) {
      // Forget any existing relationships, we don't want to make things too
      // confusing by having multiple groups active at the same time.
      ForgetAllOpeners();
    }
    data->set_opener(active_contents);
  }

  // TODO(gbillock): Ask the modal dialog manager whether the WebContents should
  // be blocked, or just let the modal dialog manager make the blocking call
  // directly and not use this at all.
  const web_modal::WebContentsModalDialogManager* manager =
      web_modal::WebContentsModalDialogManager::FromWebContents(contents);
  if (manager)
    data->set_blocked(manager->IsDialogActive());

  contents_data_.insert(contents_data_.begin() + index, std::move(data));

  selection_model_.IncrementFrom(index);

  for (auto& observer : observers())
    observer.TabInsertedAt(this, contents, index, active);

  if (active) {
    ui::ListSelectionModel new_model = selection_model_;
    new_model.SetSelectedIndex(index);
    SetSelection(std::move(new_model), Notify::kDefault);
  }
}

WebContents* TabStripModelImpl::ReplaceWebContentsAt(
    int index,
    WebContents* new_contents) {
  delegate()->WillAddWebContents(new_contents);

  DCHECK(ContainsIndex(index));
  WebContents* old_contents = GetWebContentsAtImpl(index);

  FixOpenersAndGroupsReferencing(index);

  contents_data_[index]->SetWebContents(new_contents);

  for (auto& observer : observers())
    observer.TabReplacedAt(this, old_contents, new_contents, index);

  // When the active WebContents is replaced send out a selection notification
  // too. We do this as nearly all observers need to treat a replacement of the
  // selected contents as the selection changing.
  if (active_index() == index) {
    for (auto& observer : observers()) {
      observer.ActiveTabChanged(old_contents, new_contents, active_index(),
                                TabStripModelObserver::CHANGE_REASON_REPLACED);
    }
  }
  return old_contents;
}

WebContents* TabStripModelImpl::DetachWebContentsAt(int index) {
  CHECK(!in_notify_);
  if (contents_data_.empty())
    return nullptr;
  DCHECK(ContainsIndex(index));

  FixOpenersAndGroupsReferencing(index);

  WebContents* removed_contents = GetWebContentsAtImpl(index);
  bool was_selected = IsTabSelected(index);
  int next_selected_index = order_controller_->DetermineNewSelectedIndex(index);
  contents_data_.erase(contents_data_.begin() + index);
  if (empty())
    closing_all_ = true;
  for (auto& observer : observers())
    observer.TabDetachedAt(removed_contents, index);
  if (empty()) {
    selection_model_.Clear();
    // TabDetachedAt() might unregister observers, so send |TabStripEmpty()| in
    // a second pass.
    for (auto& observer : observers())
      observer.TabStripEmpty();
  } else {
    int old_active = active_index();
    selection_model_.DecrementFrom(index);
    ui::ListSelectionModel old_model;
    old_model = selection_model_;
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
      NotifyIfActiveTabChanged(removed_contents, Notify::kDefault);
    }

    // Sending notification in case the detached tab was selected. Using
    // NotifyIfActiveOrSelectionChanged() here would not guarantee that a
    // notification is sent even though the tab selection has changed because
    // |old_model| is stored after calling DecrementFrom().
    if (was_selected) {
      for (auto& observer : observers())
        observer.TabSelectionChanged(this, old_model);
    }
  }
  return removed_contents;
}

void TabStripModelImpl::ActivateTabAt(int index, bool user_gesture) {
  DCHECK(ContainsIndex(index));
  ui::ListSelectionModel new_model = selection_model_;
  new_model.SetSelectedIndex(index);
  SetSelection(std::move(new_model),
               user_gesture ? Notify::kUserGesture : Notify::kDefault);
}

void TabStripModelImpl::AddTabAtToSelection(int index) {
  DCHECK(ContainsIndex(index));
  ui::ListSelectionModel new_model = selection_model_;
  new_model.AddIndexToSelection(index);
  SetSelection(std::move(new_model), Notify::kDefault);
}

void TabStripModelImpl::MoveWebContentsAt(int index,
                                          int to_position,
                                          bool select_after_move) {
  DCHECK(ContainsIndex(index));

  // Ensure pinned and non-pinned tabs do not mix.
  const int first_non_pinned_tab = IndexOfFirstNonPinnedTab();
  to_position = IsTabPinned(index)
                    ? std::min(first_non_pinned_tab - 1, to_position)
                    : std::max(first_non_pinned_tab, to_position);
  if (index == to_position)
    return;

  MoveWebContentsAtImpl(index, to_position, select_after_move);
}

void TabStripModelImpl::MoveSelectedTabsTo(int index) {
  int total_pinned_count = IndexOfFirstNonPinnedTab();
  int selected_pinned_count = 0;
  int selected_count =
      static_cast<int>(selection_model_.selected_indices().size());
  for (int i = 0; i < selected_count &&
                  IsTabPinned(selection_model_.selected_indices()[i]);
       ++i) {
    selected_pinned_count++;
  }

  // To maintain that all pinned tabs occur before non-pinned tabs we move them
  // first.
  if (selected_pinned_count > 0) {
    MoveSelectedTabsToImpl(
        std::min(total_pinned_count - selected_pinned_count, index), 0u,
        selected_pinned_count);
    if (index > total_pinned_count - selected_pinned_count) {
      // We're being told to drag pinned tabs to an invalid location. Adjust the
      // index such that non-pinned tabs end up at a location as though we could
      // move the pinned tabs to index. See description in header for more
      // details.
      index += selected_pinned_count;
    }
  }
  if (selected_pinned_count == selected_count)
    return;

  // Then move the non-pinned tabs.
  MoveSelectedTabsToImpl(std::max(index, total_pinned_count),
                         selected_pinned_count,
                         selected_count - selected_pinned_count);
}

WebContents* TabStripModelImpl::GetActiveWebContents() const {
  return GetWebContentsAt(active_index());
}

WebContents* TabStripModelImpl::GetWebContentsAt(int index) const {
  if (ContainsIndex(index))
    return GetWebContentsAtImpl(index);
  return nullptr;
}

int TabStripModelImpl::GetIndexOfWebContents(
    const WebContents* contents) const {
  for (size_t i = 0; i < contents_data_.size(); ++i) {
    if (contents_data_[i]->web_contents() == contents)
      return i;
  }
  return kNoTab;
}

void TabStripModelImpl::UpdateWebContentsStateAt(
    int index,
    TabStripModelObserver::TabChangeType change_type) {
  DCHECK(ContainsIndex(index));

  for (auto& observer : observers())
    observer.TabChangedAt(GetWebContentsAtImpl(index), index, change_type);
}

void TabStripModelImpl::SetTabNeedsAttentionAt(int index, bool attention) {
  DCHECK(ContainsIndex(index));

  for (auto& observer : observers())
    observer.SetTabNeedsAttentionAt(index, attention);
}

void TabStripModelImpl::CloseAllTabs() {
  // Set state so that observers can adjust their behavior to suit this
  // specific condition when CloseWebContentsAt causes a flurry of
  // Close/Detach/Select notifications to be sent.
  closing_all_ = true;
  std::vector<content::WebContents*> closing_tabs;
  closing_tabs.reserve(count());
  for (int i = count() - 1; i >= 0; --i)
    closing_tabs.push_back(GetWebContentsAt(i));
  InternalCloseTabs(closing_tabs, CLOSE_CREATE_HISTORICAL_TAB);
}

bool TabStripModelImpl::CloseWebContentsAt(int index, uint32_t close_types) {
  DCHECK(ContainsIndex(index));
  std::vector<content::WebContents*> closing_tabs(1, GetWebContentsAt(index));
  return InternalCloseTabs(closing_tabs, close_types);
}

bool TabStripModelImpl::TabsAreLoading() const {
  for (const auto& data : contents_data_) {
    if (data->web_contents()->IsLoading())
      return true;
  }

  return false;
}

WebContents* TabStripModelImpl::GetOpenerOfWebContentsAt(int index) {
  DCHECK(ContainsIndex(index));
  return contents_data_[index]->opener();
}

void TabStripModelImpl::SetOpenerOfWebContentsAt(int index,
                                                 WebContents* opener) {
  DCHECK(ContainsIndex(index));
  // The TabStripModel only maintains the references to openers that it itself
  // owns; trying to set an opener to an external WebContents can result in
  // the opener being used after its freed. See crbug.com/698681.
  DCHECK(!opener || GetIndexOfWebContents(opener) != kNoTab)
      << "Cannot set opener to a web contents not owned by this tab strip.";
  contents_data_[index]->set_opener(opener);
}

int TabStripModelImpl::GetIndexOfLastWebContentsOpenedBy(
    const WebContents* opener,
    int start_index) const {
  DCHECK(opener);
  DCHECK(ContainsIndex(start_index));

  std::set<const WebContents*> opener_and_descendants;
  opener_and_descendants.insert(opener);
  int last_index = kNoTab;

  for (int i = start_index + 1; i < count(); ++i) {
    // Test opened by transitively, i.e. include tabs opened by tabs opened by
    // opener, etc. Stop when we find the first non-descendant.
    if (!opener_and_descendants.count(contents_data_[i]->opener())) {
      // Skip over pinned tabs as new tabs are added after pinned tabs.
      if (contents_data_[i]->pinned())
        continue;
      break;
    }
    opener_and_descendants.insert(contents_data_[i]->web_contents());
    last_index = i;
  }
  return last_index;
}

void TabStripModelImpl::TabNavigating(WebContents* contents,
                                      ui::PageTransition transition) {
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

void TabStripModelImpl::SetTabBlocked(int index, bool blocked) {
  DCHECK(ContainsIndex(index));
  if (contents_data_[index]->blocked() == blocked)
    return;
  contents_data_[index]->set_blocked(blocked);
  for (auto& observer : observers())
    observer.TabBlockedStateChanged(contents_data_[index]->web_contents(),
                                    index);
}

void TabStripModelImpl::SetTabPinned(int index, bool pinned) {
  DCHECK(ContainsIndex(index));
  if (contents_data_[index]->pinned() == pinned)
    return;

  // The tab's position may have to change as the pinned tab state is changing.
  int non_pinned_tab_index = IndexOfFirstNonPinnedTab();
  contents_data_[index]->set_pinned(pinned);
  if (pinned && index != non_pinned_tab_index) {
    MoveWebContentsAtImpl(index, non_pinned_tab_index, false);
    index = non_pinned_tab_index;
  } else if (!pinned && index + 1 != non_pinned_tab_index) {
    MoveWebContentsAtImpl(index, non_pinned_tab_index - 1, false);
    index = non_pinned_tab_index - 1;
  }

  for (auto& observer : observers())
    observer.TabPinnedStateChanged(this, contents_data_[index]->web_contents(),
                                   index);
}

bool TabStripModelImpl::IsTabPinned(int index) const {
  DCHECK(ContainsIndex(index));
  return contents_data_[index]->pinned();
}

bool TabStripModelImpl::IsTabBlocked(int index) const {
  return contents_data_[index]->blocked();
}

int TabStripModelImpl::IndexOfFirstNonPinnedTab() const {
  for (size_t i = 0; i < contents_data_.size(); ++i) {
    if (!IsTabPinned(static_cast<int>(i)))
      return static_cast<int>(i);
  }
  // No pinned tabs.
  return count();
}

void TabStripModelImpl::ExtendSelectionTo(int index) {
  DCHECK(ContainsIndex(index));
  ui::ListSelectionModel new_model = selection_model_;
  new_model.SetSelectionFromAnchorTo(index);
  SetSelection(std::move(new_model), Notify::kDefault);
}

void TabStripModelImpl::ToggleSelectionAt(int index) {
  DCHECK(ContainsIndex(index));
  ui::ListSelectionModel new_model = selection_model();
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
  SetSelection(std::move(new_model), Notify::kDefault);
}

void TabStripModelImpl::AddSelectionFromAnchorTo(int index) {
  ui::ListSelectionModel new_model = selection_model_;
  new_model.AddSelectionFromAnchorTo(index);
  SetSelection(std::move(new_model), Notify::kDefault);
}

bool TabStripModelImpl::IsTabSelected(int index) const {
  DCHECK(ContainsIndex(index));
  return selection_model_.IsSelected(index);
}

void TabStripModelImpl::SetSelectionFromModel(ui::ListSelectionModel source) {
  DCHECK_NE(ui::ListSelectionModel::kUnselectedIndex, source.active());
  SetSelection(std::move(source), Notify::kDefault);
}

const ui::ListSelectionModel& TabStripModelImpl::selection_model() const {
  return selection_model_;
}

void TabStripModelImpl::AddWebContents(WebContents* contents,
                                       int index,
                                       ui::PageTransition transition,
                                       int add_types) {
  // If the newly-opened tab is part of the same task as the parent tab, we want
  // to inherit the parent's "group" attribute, so that if this tab is then
  // closed we'll jump back to the parent tab.
  bool inherit_group = (add_types & ADD_INHERIT_GROUP) == ADD_INHERIT_GROUP;

  if (ui::PageTransitionTypeIncludingQualifiersIs(transition,
                                                  ui::PAGE_TRANSITION_LINK) &&
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

  if (ui::PageTransitionTypeIncludingQualifiersIs(transition,
                                                  ui::PAGE_TRANSITION_TYPED) &&
      index == count()) {
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

  if (inherit_group && ui::PageTransitionTypeIncludingQualifiersIs(
                           transition, ui::PAGE_TRANSITION_TYPED))
    contents_data_[index]->set_reset_group_on_select(true);

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
      ResizeWebContents(contents,
                        gfx::Rect(old_contents->GetContainerBounds().size()));
    }
  }
}

void TabStripModelImpl::CloseSelectedTabs() {
  InternalCloseTabs(
      GetWebContentsesByIndices(selection_model_.selected_indices()),
      CLOSE_CREATE_HISTORICAL_TAB | CLOSE_USER_GESTURE);
}

void TabStripModelImpl::SelectNextTab() {
  SelectRelativeTab(true);
}

void TabStripModelImpl::SelectPreviousTab() {
  SelectRelativeTab(false);
}

void TabStripModelImpl::SelectLastTab() {
  ActivateTabAt(count() - 1, true);
}

void TabStripModelImpl::MoveTabNext() {
  // TODO: this likely needs to be updated for multi-selection.
  int new_index = std::min(active_index() + 1, count() - 1);
  MoveWebContentsAt(active_index(), new_index, true);
}

void TabStripModelImpl::MoveTabPrevious() {
  // TODO: this likely needs to be updated for multi-selection.
  int new_index = std::max(active_index() - 1, 0);
  MoveWebContentsAt(active_index(), new_index, true);
}

// Context menu functions.
bool TabStripModelImpl::IsContextMenuCommandEnabled(
    int context_index,
    ContextMenuCommand command_id) const {
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
        if (delegate()->CanDuplicateContentsAt(indices[i]))
          return true;
      }
      return false;
    }

    case CommandRestoreTab:
      return delegate()->GetRestoreTabType() !=
             TabStripModelDelegate::RESTORE_NONE;

    case CommandToggleTabAudioMuted:
    case CommandToggleSiteMuted: {
      TabMutedReason reason = command_id == CommandToggleSiteMuted
                                  ? TabMutedReason::CONTENT_SETTING
                                  : TabMutedReason::CONTEXT_MENU;
      std::vector<int> indices = GetIndicesForCommand(context_index);
      for (size_t i = 0; i < indices.size(); ++i) {
        if (!chrome::CanToggleAudioMute(GetWebContentsAt(indices[i]), reason))
          return false;
      }
      return true;
    }

    case CommandBookmarkAllTabs:
      return browser_defaults::bookmarks_enabled &&
             delegate()->CanBookmarkAllTabs();

    case CommandTogglePinned:
    case CommandSelectByDomain:
    case CommandSelectByOpener:
      return true;

    default:
      NOTREACHED();
  }
  return false;
}

void TabStripModelImpl::ExecuteContextMenuCommand(
    int context_index,
    ContextMenuCommand command_id) {
  DCHECK(command_id > CommandFirst && command_id < CommandLast);
  switch (command_id) {
    case CommandNewTab: {
      base::RecordAction(UserMetricsAction("TabContextMenu_NewTab"));
      UMA_HISTOGRAM_ENUMERATION("Tab.NewTab",
                                TabStripModelImpl::NEW_TAB_CONTEXT_MENU,
                                TabStripModelImpl::NEW_TAB_ENUM_COUNT);
      delegate()->AddTabAt(GURL(), context_index + 1, true);
#if BUILDFLAG(ENABLE_DESKTOP_IN_PRODUCT_HELP)
      auto* new_tab_tracker =
          feature_engagement::NewTabTrackerFactory::GetInstance()
              ->GetForProfile(profile_);
      new_tab_tracker->OnNewTabOpened();
      new_tab_tracker->CloseBubble();
#endif
      break;
    }

    case CommandReload: {
      base::RecordAction(UserMetricsAction("TabContextMenu_Reload"));
      std::vector<int> indices = GetIndicesForCommand(context_index);
      for (size_t i = 0; i < indices.size(); ++i) {
        WebContents* tab = GetWebContentsAt(indices[i]);
        if (tab) {
          CoreTabHelperDelegate* core_delegate =
              CoreTabHelper::FromWebContents(tab)->delegate();
          if (!core_delegate || core_delegate->CanReloadContents(tab))
            tab->GetController().Reload(content::ReloadType::NORMAL, true);
        }
      }
      break;
    }

    case CommandDuplicate: {
      base::RecordAction(UserMetricsAction("TabContextMenu_Duplicate"));
      std::vector<int> indices = GetIndicesForCommand(context_index);
      // Copy the WebContents off as the indices will change as tabs are
      // duplicated.
      std::vector<WebContents*> tabs;
      for (size_t i = 0; i < indices.size(); ++i)
        tabs.push_back(GetWebContentsAt(indices[i]));
      for (size_t i = 0; i < tabs.size(); ++i) {
        int index = GetIndexOfWebContents(tabs[i]);
        if (index != -1 && delegate()->CanDuplicateContentsAt(index))
          delegate()->DuplicateContentsAt(index);
      }
      break;
    }

    case CommandCloseTab: {
      base::RecordAction(UserMetricsAction("TabContextMenu_CloseTab"));
      InternalCloseTabs(
          GetWebContentsesByIndices(GetIndicesForCommand(context_index)),
          CLOSE_CREATE_HISTORICAL_TAB | CLOSE_USER_GESTURE);
      break;
    }

    case CommandCloseOtherTabs: {
      base::RecordAction(UserMetricsAction("TabContextMenu_CloseOtherTabs"));
      InternalCloseTabs(GetWebContentsesByIndices(GetIndicesClosedByCommand(
                            context_index, command_id)),
                        CLOSE_CREATE_HISTORICAL_TAB);
      break;
    }

    case CommandCloseTabsToRight: {
      base::RecordAction(UserMetricsAction("TabContextMenu_CloseTabsToRight"));
      InternalCloseTabs(GetWebContentsesByIndices(GetIndicesClosedByCommand(
                            context_index, command_id)),
                        CLOSE_CREATE_HISTORICAL_TAB);
      break;
    }

    case CommandRestoreTab: {
      base::RecordAction(UserMetricsAction("TabContextMenu_RestoreTab"));
      delegate()->RestoreTab();
      break;
    }

    case CommandTogglePinned: {
      base::RecordAction(UserMetricsAction("TabContextMenu_TogglePinned"));
      std::vector<int> indices = GetIndicesForCommand(context_index);
      bool pin = WillContextMenuPin(context_index);
      if (pin) {
        for (size_t i = 0; i < indices.size(); ++i)
          SetTabPinned(indices[i], true);
      } else {
        // Unpin from the back so that the order is maintained (unpinning can
        // trigger moving a tab).
        for (size_t i = indices.size(); i > 0; --i)
          SetTabPinned(indices[i - 1], false);
      }
      break;
    }

    case CommandToggleTabAudioMuted: {
      const std::vector<int>& indices = GetIndicesForCommand(context_index);
      const bool mute = WillContextMenuMute(context_index);
      if (mute)
        base::RecordAction(UserMetricsAction("TabContextMenu_MuteTabs"));
      else
        base::RecordAction(UserMetricsAction("TabContextMenu_UnmuteTabs"));
      for (std::vector<int>::const_iterator i = indices.begin();
           i != indices.end(); ++i) {
        chrome::SetTabAudioMuted(GetWebContentsAt(*i), mute,
                                 TabMutedReason::CONTEXT_MENU, std::string());
      }
      break;
    }

    case CommandToggleSiteMuted: {
      const std::vector<int>& indices = GetIndicesForCommand(context_index);
      const bool mute = WillContextMenuMuteSites(context_index);
      if (mute) {
        base::RecordAction(
            UserMetricsAction("SoundContentSetting.MuteBy.TabStrip"));
      } else {
        base::RecordAction(
            UserMetricsAction("SoundContentSetting.UnmuteBy.TabStrip"));
      }
      chrome::SetSitesMuted(*this, indices, mute);
      break;
    }

    case CommandBookmarkAllTabs: {
      base::RecordAction(UserMetricsAction("TabContextMenu_BookmarkAllTabs"));

      delegate()->BookmarkAllTabs();
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
      SetSelectionFromModel(std::move(selection_model));
      break;
    }

    default:
      NOTREACHED();
  }
}

std::vector<int> TabStripModelImpl::GetIndicesClosedByCommand(
    int index,
    ContextMenuCommand id) const {
  DCHECK(ContainsIndex(index));
  DCHECK(id == CommandCloseTabsToRight || id == CommandCloseOtherTabs);
  bool is_selected = IsTabSelected(index);
  int last_unclosed_tab = -1;
  if (id == CommandCloseTabsToRight) {
    last_unclosed_tab =
        is_selected ? selection_model_.selected_indices().back() : index;
  }

  // NOTE: callers expect the vector to be sorted in descending order.
  std::vector<int> indices;
  for (int i = count() - 1; i > last_unclosed_tab; --i) {
    if (i != index && !IsTabPinned(i) && (!is_selected || !IsTabSelected(i)))
      indices.push_back(i);
  }
  return indices;
}

bool TabStripModelImpl::WillContextMenuMute(int index) {
  std::vector<int> indices = GetIndicesForCommand(index);
  return !chrome::AreAllTabsMuted(*this, indices);
}

bool TabStripModelImpl::WillContextMenuMuteSites(int index) {
  return !chrome::AreAllSitesMuted(*this, GetIndicesForCommand(index));
}

bool TabStripModelImpl::WillContextMenuPin(int index) {
  std::vector<int> indices = GetIndicesForCommand(index);
  // If all tabs are pinned, then we unpin, otherwise we pin.
  bool all_pinned = true;
  for (size_t i = 0; i < indices.size() && all_pinned; ++i)
    all_pinned = IsTabPinned(indices[i]);
  return !all_pinned;
}

int TabStripModelImpl::GetIndexOfNextWebContentsOpenedBy(
    const WebContents* opener,
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

void TabStripModelImpl::ForgetAllOpeners() {
  // Forget all opener memories so we don't do anything weird with tab
  // re-selection ordering.
  for (const auto& data : contents_data_)
    data->set_opener(nullptr);
}

void TabStripModelImpl::ForgetGroup(WebContents* contents) {
  int index = GetIndexOfWebContents(contents);
  DCHECK(ContainsIndex(index));
  contents_data_[index]->set_group(nullptr);
  contents_data_[index]->set_opener(nullptr);
}

bool TabStripModelImpl::ShouldResetGroupOnSelect(WebContents* contents) const {
  int index = GetIndexOfWebContents(contents);
  DCHECK(ContainsIndex(index));
  return contents_data_[index]->reset_group_on_select();
}

///////////////////////////////////////////////////////////////////////////////
// TabStripModel, private:

int TabStripModelImpl::ConstrainInsertionIndex(int index, bool pinned_tab) {
  return pinned_tab
             ? std::min(std::max(0, index), IndexOfFirstNonPinnedTab())
             : std::min(count(), std::max(index, IndexOfFirstNonPinnedTab()));
}

std::vector<WebContents*> TabStripModelImpl::GetWebContentsFromIndices(
    const std::vector<int>& indices) const {
  std::vector<WebContents*> contents;
  for (size_t i = 0; i < indices.size(); ++i)
    contents.push_back(GetWebContentsAtImpl(indices[i]));
  return contents;
}

void TabStripModelImpl::GetIndicesWithSameDomain(int index,
                                                 std::vector<int>* indices) {
  std::string domain = GetWebContentsAt(index)->GetURL().host();
  if (domain.empty())
    return;
  for (int i = 0; i < count(); ++i) {
    if (i == index)
      continue;
    if (GetWebContentsAt(i)->GetURL().host_piece() == domain)
      indices->push_back(i);
  }
}

void TabStripModelImpl::GetIndicesWithSameOpener(int index,
                                                 std::vector<int>* indices) {
  WebContents* opener = contents_data_[index]->group();
  if (!opener) {
    // If there is no group, find all tabs with the selected tab as the opener.
    opener = GetWebContentsAt(index);
    if (!opener)
      return;
  }
  for (int i = 0; i < count(); ++i) {
    if (i == index)
      continue;
    if (contents_data_[i]->group() == opener ||
        GetWebContentsAtImpl(i) == opener) {
      indices->push_back(i);
    }
  }
}

std::vector<int> TabStripModelImpl::GetIndicesForCommand(int index) const {
  if (!IsTabSelected(index)) {
    std::vector<int> indices;
    indices.push_back(index);
    return indices;
  }
  return selection_model_.selected_indices();
}

bool TabStripModelImpl::IsNewTabAtEndOfTabStrip(WebContents* contents) const {
  const GURL& url = contents->GetURL();
  return url.SchemeIs(content::kChromeUIScheme) &&
         url.host_piece() == chrome::kChromeUINewTabHost &&
         contents == GetWebContentsAtImpl(count() - 1) &&
         contents->GetController().GetEntryCount() == 1;
}

std::vector<content::WebContents*> TabStripModelImpl::GetWebContentsesByIndices(
    const std::vector<int>& indices) {
  std::vector<content::WebContents*> items;
  items.reserve(indices.size());
  for (int index : indices)
    items.push_back(GetWebContentsAtImpl(index));
  return items;
}

bool TabStripModelImpl::InternalCloseTabs(
    const std::vector<content::WebContents*>& items,
    uint32_t close_types) {
  if (items.empty())
    return true;

  const bool closing_all = static_cast<int>(items.size()) == count();
  base::WeakPtr<TabStripModelImpl> ref = weak_factory_.GetWeakPtr();
  if (closing_all) {
    for (auto& observer : observers())
      observer.WillCloseAllTabs();
  }
  const bool closed_all = CloseWebContentses(this, items, close_types);
  if (!ref)
    return closed_all;
  if (closing_all && !closed_all) {
    for (auto& observer : observers())
      observer.CloseAllTabsCanceled();
  }
  return closed_all;
}

WebContents* TabStripModelImpl::GetWebContentsAtImpl(int index) const {
  CHECK(ContainsIndex(index))
      << "Failed to find: " << index << " in: " << count() << " entries.";
  return contents_data_[index]->web_contents();
}

void TabStripModelImpl::NotifyIfTabDeactivated(WebContents* contents) {
  if (contents) {
    for (auto& observer : observers())
      observer.TabDeactivated(contents);
  }
}

void TabStripModelImpl::NotifyIfActiveTabChanged(WebContents* old_contents,
                                                 Notify notify_types) {
  WebContents* new_contents = GetWebContentsAtImpl(active_index());
  if (old_contents == new_contents)
    return;

  int reason = notify_types == Notify::kUserGesture
                   ? TabStripModelObserver::CHANGE_REASON_USER_GESTURE
                   : TabStripModelObserver::CHANGE_REASON_NONE;
  CHECK(!in_notify_);
  in_notify_ = true;
  for (auto& observer : observers()) {
    observer.ActiveTabChanged(old_contents, new_contents, active_index(),
                              reason);
  }
  in_notify_ = false;
}

void TabStripModelImpl::NotifyIfActiveOrSelectionChanged(
    WebContents* old_contents,
    Notify notify_types,
    const ui::ListSelectionModel& old_model) {
  NotifyIfActiveTabChanged(old_contents, notify_types);

  if (selection_model() != old_model) {
    for (auto& observer : observers())
      observer.TabSelectionChanged(this, old_model);
  }
}

void TabStripModelImpl::SetSelection(ui::ListSelectionModel new_model,
                                     Notify notify_types) {
  WebContents* old_contents = GetActiveWebContents();
  ui::ListSelectionModel old_model;
  old_model = selection_model_;
  if (new_model.active() != selection_model_.active())
    NotifyIfTabDeactivated(old_contents);
  selection_model_ = new_model;
  NotifyIfActiveOrSelectionChanged(old_contents, notify_types, old_model);
}

void TabStripModelImpl::SelectRelativeTab(bool next) {
  // This may happen during automated testing or if a user somehow buffers
  // many key accelerators.
  if (contents_data_.empty())
    return;

  int index = active_index();
  int delta = next ? 1 : -1;
  index = (index + count() + delta) % count();
  ActivateTabAt(index, true);
}

void TabStripModelImpl::MoveWebContentsAtImpl(int index,
                                              int to_position,
                                              bool select_after_move) {
  FixOpenersAndGroupsReferencing(index);

  std::unique_ptr<WebContentsData> moved_data =
      std::move(contents_data_[index]);
  WebContents* web_contents = moved_data->web_contents();
  contents_data_.erase(contents_data_.begin() + index);
  contents_data_.insert(contents_data_.begin() + to_position,
                        std::move(moved_data));

  selection_model_.Move(index, to_position, 1);
  if (!selection_model_.IsSelected(to_position) && select_after_move) {
    // TODO(sky): why doesn't this code notify observers?
    selection_model_.SetSelectedIndex(to_position);
  }

  for (auto& observer : observers())
    observer.TabMoved(web_contents, index, to_position);
}

void TabStripModelImpl::MoveSelectedTabsToImpl(int index,
                                               size_t start,
                                               size_t length) {
  DCHECK(start < selection_model_.selected_indices().size() &&
         start + length <= selection_model_.selected_indices().size());
  size_t end = start + length;
  int count_before_index = 0;
  for (size_t i = start; i < end && selection_model_.selected_indices()[i] <
                                        index + count_before_index;
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
bool TabStripModelImpl::OpenerMatches(
    const std::unique_ptr<WebContentsData>& data,
    const WebContents* opener,
    bool use_group) {
  return data->opener() == opener || (use_group && data->group() == opener);
}

void TabStripModelImpl::FixOpenersAndGroupsReferencing(int index) {
  WebContents* old_contents = GetWebContentsAtImpl(index);
  for (auto& data : contents_data_) {
    if (data->group() == old_contents)
      data->set_group(contents_data_[index]->group());
    if (data->opener() == old_contents)
      data->set_opener(contents_data_[index]->opener());
  }
}
