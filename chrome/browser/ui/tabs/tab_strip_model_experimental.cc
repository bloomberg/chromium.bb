// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_strip_model_experimental.h"

#include <memory>

#include "base/containers/flat_map.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/browser_shutdown.h"
#include "chrome/browser/ui/tab_contents/core_tab_helper.h"
#include "chrome/browser/ui/tabs/tab_data_experimental.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/tabs/tab_strip_model_experimental_observer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/web_contents_observer.h"

namespace {

// CloseTracker is used when closing a set of WebContents. It listens for
// deletions of the WebContents and removes from the internal set any time one
// is deleted.
//
// TODO(brettw) how to tear down a tab should not be in the tab strip. This is
// duplicated from tab_strip_model_impl.cc. WebContents teardown should be
// somewhere more fundamental.
class CloseTracker {
 public:
  using Contents = std::vector<content::WebContents*>;

  explicit CloseTracker(base::span<content::WebContents*> contents);
  ~CloseTracker();

  // Returns true if there is another WebContents in the Tracker.
  bool HasNext() const;

  // Returns the next WebContents, or NULL if there are no more.
  content::WebContents* Next();

 private:
  class DeletionObserver : public content::WebContentsObserver {
   public:
    DeletionObserver(CloseTracker* parent, content::WebContents* web_contents)
        : WebContentsObserver(web_contents), parent_(parent) {}

   private:
    // WebContentsObserver:
    void WebContentsDestroyed() override {
      parent_->OnWebContentsDestroyed(this);
    }

    CloseTracker* parent_;

    DISALLOW_COPY_AND_ASSIGN(DeletionObserver);
  };

  void OnWebContentsDestroyed(DeletionObserver* observer);

  using Observers = std::vector<std::unique_ptr<DeletionObserver>>;
  Observers observers_;

  DISALLOW_COPY_AND_ASSIGN(CloseTracker);
};

CloseTracker::CloseTracker(base::span<content::WebContents*> contents) {
  observers_.reserve(contents.size());
  for (content::WebContents* current : contents)
    observers_.push_back(base::MakeUnique<DeletionObserver>(this, current));
}

CloseTracker::~CloseTracker() {
  DCHECK(observers_.empty());
}

bool CloseTracker::HasNext() const {
  return !observers_.empty();
}

content::WebContents* CloseTracker::Next() {
  if (observers_.empty())
    return nullptr;

  DeletionObserver* observer = observers_[0].get();
  content::WebContents* web_contents = observer->web_contents();
  observers_.erase(observers_.begin());
  return web_contents;
}

void CloseTracker::OnWebContentsDestroyed(DeletionObserver* observer) {
  for (auto i = observers_.begin(); i != observers_.end(); ++i) {
    if (observer == i->get()) {
      observers_.erase(i);
      return;
    }
  }
  NOTREACHED() << "WebContents destroyed that wasn't in the list";
}

}  // namespace

// TabStripModelExperimental ---------------------------------------------------

TabStripModelExperimental::TabStripModelExperimental(
    TabStripModelDelegate* delegate,
    Profile* profile)
    : delegate_(delegate), profile_(profile), weak_factory_(this) {}

TabStripModelExperimental::~TabStripModelExperimental() {}

TabStripModelExperimental*
TabStripModelExperimental::AsTabStripModelExperimental() {
  return this;
}

TabStripModelDelegate* TabStripModelExperimental::delegate() const {
  return delegate_;
}

void TabStripModelExperimental::AddObserver(TabStripModelObserver* observer) {
  observers_.AddObserver(observer);
}

void TabStripModelExperimental::RemoveObserver(
    TabStripModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

int TabStripModelExperimental::count() const {
  return static_cast<int>(tabs_.size());
}

bool TabStripModelExperimental::empty() const {
  return tabs_.empty();
}

Profile* TabStripModelExperimental::profile() const {
  return profile_;
}

int TabStripModelExperimental::active_index() const {
  return selection_model_.active();
}

bool TabStripModelExperimental::closing_all() const {
  return closing_all_;
}

bool TabStripModelExperimental::ContainsIndex(int index) const {
  return index >= 0 && index < count();
}

void TabStripModelExperimental::AppendWebContents(
    content::WebContents* contents,
    bool foreground) {
  InsertWebContentsAt(count(), contents,
                      foreground ? (ADD_INHERIT_GROUP | ADD_ACTIVE) : ADD_NONE);
}

void TabStripModelExperimental::InsertWebContentsAt(
    int index,
    content::WebContents* contents,
    int add_types) {
  delegate_->WillAddWebContents(contents);

  bool active = (add_types & ADD_ACTIVE) != 0;

  // Always insert tabs at the end for now.
  index = count();

  tabs_.emplace(tabs_.begin() + index, contents, this);
  selection_model_.IncrementFrom(index);

  for (auto& observer : exp_observers_)
    observer.TabInsertedAt(index, active);
  for (auto& observer : observers_)
    observer.TabInsertedAt(this, contents, index, active);

  if ((add_types & ADD_ACTIVE) != 0) {
    ui::ListSelectionModel new_model = selection_model_;
    new_model.SetSelectedIndex(index);
    SetSelection(std::move(new_model), Notify::kDefault);
  }
}

bool TabStripModelExperimental::CloseWebContentsAt(int index,
                                                   uint32_t close_types) {
  DCHECK(tabs_[index].type() == TabDataExperimental::Type::kSingle);
  content::WebContents* closing = tabs_[index].contents_;
  InternalCloseTabs(base::span<content::WebContents*>(&closing, 1));
  return true;
}

content::WebContents* TabStripModelExperimental::ReplaceWebContentsAt(
    int index,
    content::WebContents* new_contents) {
  delegate_->WillAddWebContents(new_contents);

  DCHECK(ContainsIndex(index));
  content::WebContents* old_contents = tabs_[index].contents_;

  tabs_[index].ReplaceContents(new_contents);

  for (auto& observer : observers_)
    observer.TabReplacedAt(this, old_contents, new_contents, index);

  // When the active WebContents is replaced send out a selection notification
  // too. We do this as nearly all observers need to treat a replacement of the
  // selected contents as the selection changing.
  if (active_index() == index) {
    for (auto& observer : observers_)
      observer.ActiveTabChanged(old_contents, new_contents, active_index(),
                                TabStripModelObserver::CHANGE_REASON_REPLACED);
  }
  return old_contents;
}

content::WebContents* TabStripModelExperimental::DetachWebContentsAt(
    int index) {
  CHECK(!in_notify_);
  DCHECK(ContainsIndex(index));

  content::WebContents* removed_contents = tabs_[index].contents_;
  bool was_selected = IsTabSelected(index);
  int next_selected_index = index;

  tabs_.erase(tabs_.begin() + index);
  if (next_selected_index >= static_cast<int>(tabs_.size()))
    next_selected_index = static_cast<int>(tabs_.size()) - 1;

  if (empty())
    closing_all_ = true;

  for (auto& observer : observers_)
    observer.TabDetachedAt(removed_contents, index);

  if (empty()) {
    selection_model_.Clear();
    // TabDetachedAt() might unregister observers, so send |TabStripEmpty()| in
    // a second pass.
    for (auto& observer : observers_)
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
      for (auto& observer : observers_)
        observer.TabSelectionChanged(this, old_model);
    }
  }
  return removed_contents;
}

void TabStripModelExperimental::ActivateTabAt(int index, bool user_gesture) {
  DCHECK(ContainsIndex(index));
  ui::ListSelectionModel new_model = selection_model_;
  new_model.SetSelectedIndex(index);
  SetSelection(std::move(new_model),
               user_gesture ? Notify::kUserGesture : Notify::kDefault);
}

void TabStripModelExperimental::AddTabAtToSelection(int index) {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::MoveWebContentsAt(int index,
                                                  int to_position,
                                                  bool select_after_move) {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::MoveSelectedTabsTo(int index) {
  NOTIMPLEMENTED();
}

content::WebContents* TabStripModelExperimental::GetActiveWebContents() const {
  return GetWebContentsAt(active_index());
}

content::WebContents* TabStripModelExperimental::GetWebContentsAt(
    int index) const {
  if (ContainsIndex(index))
    return tabs_[index].contents_;
  return nullptr;
}

int TabStripModelExperimental::GetIndexOfWebContents(
    const content::WebContents* contents) const {
  for (size_t i = 0; i < tabs_.size(); i++) {
    if (tabs_[i].contents_ == contents)
      return i;
  }
  return kNoTab;
}

void TabStripModelExperimental::UpdateWebContentsStateAt(
    int index,
    TabStripModelObserver::TabChangeType change_type) {
  const TabDataExperimental& data = tabs_[index];

  for (auto& observer : exp_observers_)
    observer.TabChangedAt(index, data, change_type);
}

void TabStripModelExperimental::SetTabNeedsAttentionAt(int index,
                                                       bool attention) {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::CloseAllTabs() {
  if (tabs_.empty())
    return;  // Don't issue closing callbacks if there are no tabs.

  closing_all_ = true;

  std::vector<content::WebContents*> closing;
  closing.reserve(tabs_.size());
  for (const auto& tab : tabs_)
    closing.push_back(tab.contents_);

  InternalCloseTabs(closing);
}

bool TabStripModelExperimental::TabsAreLoading() const {
  NOTIMPLEMENTED();
  return false;
}

content::WebContents* TabStripModelExperimental::GetOpenerOfWebContentsAt(
    int index) {
  NOTIMPLEMENTED();
  return nullptr;
}

void TabStripModelExperimental::SetOpenerOfWebContentsAt(
    int index,
    content::WebContents* opener) {
  NOTIMPLEMENTED();
}

int TabStripModelExperimental::GetIndexOfLastWebContentsOpenedBy(
    const content::WebContents* opener,
    int start_index) const {
  return kNoTab;
}

void TabStripModelExperimental::TabNavigating(content::WebContents* contents,
                                              ui::PageTransition transition) {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::SetTabBlocked(int index, bool blocked) {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::SetTabPinned(int index, bool pinned) {
  NOTIMPLEMENTED();
}

bool TabStripModelExperimental::IsTabPinned(int index) const {
  NOTIMPLEMENTED();
  return false;
}

bool TabStripModelExperimental::IsTabBlocked(int index) const {
  NOTIMPLEMENTED();
  return false;
}

int TabStripModelExperimental::IndexOfFirstNonPinnedTab() const {
  NOTIMPLEMENTED();
  return 0;
}

void TabStripModelExperimental::ExtendSelectionTo(int index) {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::ToggleSelectionAt(int index) {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::AddSelectionFromAnchorTo(int index) {
  NOTIMPLEMENTED();
}

bool TabStripModelExperimental::IsTabSelected(int index) const {
  DCHECK(ContainsIndex(index));
  return selection_model_.IsSelected(index);
}

void TabStripModelExperimental::SetSelectionFromModel(
    ui::ListSelectionModel source) {
  NOTIMPLEMENTED();
}

const ui::ListSelectionModel& TabStripModelExperimental::selection_model()
    const {
  return selection_model_;
}

void TabStripModelExperimental::AddWebContents(content::WebContents* contents,
                                               int index,
                                               ui::PageTransition transition,
                                               int add_types) {
  InsertWebContentsAt(index, contents, add_types);
}

void TabStripModelExperimental::CloseSelectedTabs() {
  std::vector<content::WebContents*> closed_contents;
  for (int index : selection_model_.selected_indices())
    closed_contents.push_back(tabs_[index].contents_);
  InternalCloseTabs(closed_contents);
}

void TabStripModelExperimental::SelectNextTab() {
  ActivateTabAt((active_index() + 1) % count(), true);
}

void TabStripModelExperimental::SelectPreviousTab() {
  ActivateTabAt((count() + active_index() - 1) % count(), true);
}

void TabStripModelExperimental::SelectLastTab() {
  ActivateTabAt(count() - 1, true);
}

void TabStripModelExperimental::MoveTabNext() {
  NOTIMPLEMENTED();
}

void TabStripModelExperimental::MoveTabPrevious() {
  NOTIMPLEMENTED();
}

bool TabStripModelExperimental::IsContextMenuCommandEnabled(
    int context_index,
    ContextMenuCommand command_id) const {
  NOTIMPLEMENTED();
  return false;
}

void TabStripModelExperimental::ExecuteContextMenuCommand(
    int context_index,
    ContextMenuCommand command_id) {
  NOTIMPLEMENTED();
}

std::vector<int> TabStripModelExperimental::GetIndicesClosedByCommand(
    int index,
    ContextMenuCommand id) const {
  NOTIMPLEMENTED();
  return std::vector<int>();
}

bool TabStripModelExperimental::WillContextMenuMute(int index) {
  NOTIMPLEMENTED();
  return false;
}

bool TabStripModelExperimental::WillContextMenuMuteSites(int index) {
  NOTIMPLEMENTED();
  return false;
}

bool TabStripModelExperimental::WillContextMenuPin(int index) {
  NOTIMPLEMENTED();
  return false;
}

const TabDataExperimental& TabStripModelExperimental::GetDataAt(
    int index) const {
  return tabs_[index];
}

void TabStripModelExperimental::AddExperimentalObserver(
    TabStripModelExperimentalObserver* observer) {
  exp_observers_.AddObserver(observer);
}

void TabStripModelExperimental::RemoveExperimentalObserver(
    TabStripModelExperimentalObserver* observer) {
  exp_observers_.RemoveObserver(observer);
}

void TabStripModelExperimental::SetSelection(ui::ListSelectionModel new_model,
                                             Notify notify_types) {
  content::WebContents* old_contents = GetActiveWebContents();
  ui::ListSelectionModel old_model;
  old_model = selection_model_;
  if (new_model.active() != selection_model_.active())
    NotifyIfTabDeactivated(old_contents);
  selection_model_ = new_model;
  NotifyIfActiveOrSelectionChanged(old_contents, notify_types, old_model);
}

void TabStripModelExperimental::NotifyIfTabDeactivated(
    content::WebContents* contents) {
  if (contents) {
    for (auto& observer : observers_)
      observer.TabDeactivated(contents);
  }
}

void TabStripModelExperimental::NotifyIfActiveTabChanged(
    content::WebContents* old_contents,
    Notify notify_types) {
  content::WebContents* new_contents = tabs_[active_index()].contents_;
  if (old_contents == new_contents)
    return;

  int reason = notify_types == Notify::kUserGesture
                   ? TabStripModelObserver::CHANGE_REASON_USER_GESTURE
                   : TabStripModelObserver::CHANGE_REASON_NONE;
  CHECK(!in_notify_);
  in_notify_ = true;
  for (auto& observer : observers_) {
    observer.ActiveTabChanged(old_contents, new_contents, active_index(),
                              reason);
  }
  in_notify_ = false;
}

void TabStripModelExperimental::NotifyIfActiveOrSelectionChanged(
    content::WebContents* old_contents,
    Notify notify_types,
    const ui::ListSelectionModel& old_model) {
  NotifyIfActiveTabChanged(old_contents, notify_types);

  if (selection_model() != old_model) {
    for (auto& observer : exp_observers_)
      observer.TabSelectionChanged(old_model, selection_model());
    for (auto& observer : observers_)
      observer.TabSelectionChanged(this, old_model);
  }
}

void TabStripModelExperimental::InternalCloseTabs(
    base::span<content::WebContents*> tabs_to_close) {
  CloseTracker close_tracker(tabs_to_close);

  base::WeakPtr<TabStripModel> ref(weak_factory_.GetWeakPtr());
  const bool closing_all = tabs_.size() == tabs_to_close.size();
  if (closing_all) {
    for (auto& observer : observers_)
      observer.WillCloseAllTabs();
  }

  // TODO(brettw) how to tear down a tab should not be in the tab strip. This
  // is duplicated from tab_strip_model_impl.cc. WebContents teardown should be
  // somewhere more fundamental.

  // We only try the fast shutdown path if the whole browser process is *not*
  // shutting down. Fast shutdown during browser termination is handled in
  // browser_shutdown::OnShutdownStarting.
  if (browser_shutdown::GetShutdownType() == browser_shutdown::NOT_VALID) {
    // Construct a map of processes to the number of associated tabs that are
    // closing.
    base::flat_map<content::RenderProcessHost*, size_t> processes;
    for (content::WebContents* closing_contents : tabs_to_close) {
      if (delegate_->ShouldRunUnloadListenerBeforeClosing(closing_contents))
        continue;
      content::RenderProcessHost* process =
          closing_contents->GetMainFrame()->GetProcess();
      ++processes[process];
    }

    // Try to fast shutdown the tabs that can close.
    for (const auto& pair : processes)
      pair.first->FastShutdownIfPossible(pair.second, false);
  }

  // We now return to our regularly scheduled shutdown procedure.
  bool retval = true;
  while (close_tracker.HasNext()) {
    content::WebContents* closing_contents = close_tracker.Next();
    int index = GetIndexOfWebContents(closing_contents);
    // Make sure we still contain the tab.
    if (index == kNoTab)
      continue;

    CoreTabHelper* core_tab_helper =
        CoreTabHelper::FromWebContents(closing_contents);
    core_tab_helper->OnCloseStarted();

    // Update the explicitly closed state. If the unload handlers cancel the
    // close the state is reset in Browser. We don't update the explicitly
    // closed state if already marked as explicitly closed as unload handlers
    // call back to this if the close is allowed.
    if (!closing_contents->GetClosedByUserGesture()) {
      // TODO(brettw) set parameter.
      closing_contents->SetClosedByUserGesture(true);
    }

    if (delegate_->RunUnloadListenerBeforeClosing(closing_contents)) {
      retval = false;
      continue;
    }

    for (auto& observer : exp_observers_)
      observer.TabClosedAt(index);
    for (auto& observer : observers_)
      observer.TabClosingAt(this, closing_contents, index);

    // Ask the delegate to save an entry for this tab in the historical tab
    // database if applicable.
    // if ((close_types & CLOSE_CREATE_HISTORICAL_TAB) != 0)
    delegate_->CreateHistoricalTab(closing_contents);

    // Deleting the WebContents will call back to us via
    // WebContentsData::WebContentsDestroyed and detach it.
    delete closing_contents;
  }

  if (ref && closing_all && !retval) {
    for (auto& observer : observers_)
      observer.CloseAllTabsCanceled();
  }
}
