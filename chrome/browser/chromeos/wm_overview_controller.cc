// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/wm_overview_controller.h"

#include <algorithm>
#include <vector>

#include "base/linked_ptr.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/chromeos/wm_overview_snapshot.h"
#include "chrome/browser/renderer_host/render_view_host.h"
#include "chrome/browser/renderer_host/render_widget_host.h"
#include "chrome/browser/renderer_host/render_widget_host_view.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view.h"
#include "chrome/browser/tab_contents/thumbnail_generator.h"
#include "chrome/browser/views/frame/browser_extender.h"
#include "chrome/browser/views/frame/browser_view.h"
#include "chrome/common/notification_service.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"
#include "views/window/window.h"

using std::vector;

#if !defined(OS_CHROMEOS)
#error This file is only meant to be compiled for ChromeOS
#endif

namespace chromeos {

// The time that the delay timer waits before starting the
// configuration pass.
//
// NOTE(gspencer): Yes, this is a pretty short delay, and I could
// remove it and just use the configure timer.  The reason I'm not
// doing that is that I have a hunch that we'll be needing to tune
// things further, and this will be one of the knobs.
static const int kDelayTimeMs = 10;

// The time between setting snapshots during the configuration pass,
// so that the CPU has a chance to do something else (to keep overview
// mode responsive).
static const int kConfigureTimeMs = 10;

class BrowserListener : public TabStripModelObserver {
 public:
  BrowserListener(Browser* browser, WmOverviewController* parent);
  ~BrowserListener();

  // Begin TabStripModelObserver methods
  virtual void TabInsertedAt(TabContents* contents,
                             int index,
                             bool foreground);
  virtual void TabClosingAt(TabContents* contents, int index) {}
  virtual void TabDetachedAt(TabContents* contents, int index);
  virtual void TabMoved(TabContents* contents,
                        int from_index,
                        int to_index);
  virtual void TabChangedAt(TabContents* contents, int index,
                            TabStripModelObserver::TabChangeType change_type);
  virtual void TabStripEmpty();
  virtual void TabDeselectedAt(TabContents* contents, int index) {}
  virtual void TabSelectedAt(TabContents* old_contents,
                             TabContents* new_contents,
                             int index,
                             bool user_gesture);
  // End TabStripModelObserver methods

  // Returns the number of tabs in this child.
  int count() const { return browser_->tabstrip_model()->count(); }

  // Returns the browser that this child gets its data from.
  Browser* browser() const { return browser_; }

  // Removes all the snapshots and re-populates them from the browser.
  void RecreateSnapshots();

  // Mark the given snapshot as dirty, and start the delay timer.
  void ReloadSnapshot(int index);

  // Updates the selected index and tab count on the toplevel window.
  void UpdateSelectedIndex(int index);

  // Finds the first cell with no snapshot and invokes ConfigureCell
  // for it.  Returns false if there are no more cells to configure on
  // this listener.
  bool ConfigureNextUnconfiguredSnapshot();

  // Saves the currently selected tab.
  void SaveCurrentTab() { original_selected_tab_ = browser_->selected_index(); }

  // Reverts the selected browser tab to the tab that was selected
  // when This BrowserListener was created, or the last time
  // SaveCurrentTab was called.
  void RestoreOriginalSelectedTab();

  // Selects the tab at the given index.
  void SelectTab(int index, uint32 timestamp);

  // Shows any snapshots that are not visible.
  void ShowSnapshots();

  // Returns the tab contents from the tab model for this child at index.
  TabContents* GetTabContentsAt(int index) const  {
    return browser_->tabstrip_model()->GetTabContentsAt(index);
  }
 private:

  // Configures a cell from the tab contents.
  void ConfigureCell(WmOverviewSnapshot* cell, TabContents* contents);

  // Configures a cell from the model.
  void ConfigureCell(WmOverviewSnapshot* cell, int index) {
    ConfigureCell(cell, GetTabContentsAt(index));
  }

  // Inserts a new snapshot, initialized from the model, at the given
  // index, and renumbers any following snapshots.
  void InsertSnapshot(int index);

  // Removes the snapshot at index.
  void ClearSnapshot(int index);

  // Renumbers the index atom in the snapshots starting at the given
  // index.
  void RenumberSnapshots(int start_index);

  Browser* browser_;  // Not owned
  WmOverviewController* controller_;  // Not owned

  // Widgets containing snapshot images for this browser.
  typedef std::vector<WmOverviewSnapshot* > SnapshotVector;
  SnapshotVector snapshots_;

  // True if the snapshots are showing.
  bool snapshots_showing_;

  // Non-zero if we are currently setting the tab from within SelectTab.
  // This is used to make sure we use the right timestamp when sending
  // property changes that originated from the window manager.
  uint32 select_tab_timestamp_;

  // The tab selected the last time SaveCurrentTab is called.
  int original_selected_tab_;

  DISALLOW_COPY_AND_ASSIGN(BrowserListener);
};

BrowserListener::BrowserListener(Browser* browser,
                                 WmOverviewController* controller)
    : browser_(browser),
      controller_(controller),
      snapshots_showing_(false),
      select_tab_timestamp_(0),
      original_selected_tab_(-1) {
  CHECK(browser_);
  CHECK(controller_);

  browser_->tabstrip_model()->AddObserver(this);

  // This browser didn't already exist, and so we haven't been
  // watching it for tab insertions, so we need to create the
  // snapshots associated with it.
  RecreateSnapshots();
}

BrowserListener::~BrowserListener() {
  browser_->tabstrip_model()->RemoveObserver(this);
}

void BrowserListener::TabInsertedAt(TabContents* contents,
                                    int index,
                                    bool foreground) {
  InsertSnapshot(index);
  RenumberSnapshots(index);
  UpdateSelectedIndex(browser_->selected_index());
}

void BrowserListener::TabDetachedAt(TabContents* contents, int index) {
  ClearSnapshot(index);
  UpdateSelectedIndex(browser_->selected_index());
  RenumberSnapshots(index);
}

void BrowserListener::TabMoved(TabContents* contents,
                               int from_index,
                               int to_index) {
  // Need to reorder tab in the snapshots list, and reset the window
  // type atom on the affected snapshots (the one moved, and all the
  // ones after it), so that their indices are correct.
  WmOverviewSnapshot* snapshot = snapshots_[from_index];
  snapshots_.erase(snapshots_.begin() + from_index);
  snapshots_.insert(snapshots_.begin() + to_index, snapshot);

  RenumberSnapshots(std::min(to_index, from_index));
}

void BrowserListener::TabChangedAt(
    TabContents* contents,
    int index,
    TabStripModelObserver::TabChangeType change_type) {
  if (change_type != TabStripModelObserver::LOADING_ONLY &&
      change_type != TabStripModelObserver::TITLE_NOT_LOADING) {
    ReloadSnapshot(index);
  }
}

void BrowserListener::TabStripEmpty() {
  snapshots_.clear();
}

void BrowserListener::TabSelectedAt(TabContents* old_contents,
                                    TabContents* new_contents,
                                    int index,
                                    bool user_gesture) {
  UpdateSelectedIndex(index);
}

void BrowserListener::ReloadSnapshot(int index) {
  snapshots_[index]->reload_snapshot();
  controller_->StartDelayTimer();
}

void BrowserListener::RecreateSnapshots() {
  snapshots_.clear();

  for (int i = 0; i < count(); ++i) {
    InsertSnapshot(i);
  }

  RenumberSnapshots(0);
}

void BrowserListener::UpdateSelectedIndex(int index) {
  WmIpcWindowType type = WmIpc::instance()->GetWindowType(
      GTK_WIDGET(browser_->window()->GetNativeHandle()), NULL);
  // Make sure we only operate on toplevel windows.
  if (type == WM_IPC_WINDOW_CHROME_TOPLEVEL) {
    std::vector<int> params;
    params.push_back(browser_->tab_count());
    params.push_back(index);
    params.push_back(select_tab_timestamp_ ? select_tab_timestamp_ :
                     gtk_get_current_event_time());
    WmIpc::instance()->SetWindowType(
        GTK_WIDGET(browser_->window()->GetNativeHandle()),
        WM_IPC_WINDOW_CHROME_TOPLEVEL,
        &params);
  }
}

bool BrowserListener::ConfigureNextUnconfiguredSnapshot() {
  for (SnapshotVector::size_type i = 0; i < snapshots_.size(); ++i) {
    WmOverviewSnapshot* cell = snapshots_[i];
    if (!cell->configured_snapshot()) {
      ConfigureCell(cell, i);
      return true;
    }
  }
  return false;
}

void BrowserListener::RestoreOriginalSelectedTab() {
  if (original_selected_tab_ >= 0) {
    browser_->SelectTabContentsAt(original_selected_tab_, false);
  }
}

void BrowserListener::ShowSnapshots() {
  for (SnapshotVector::size_type i = 0; i < snapshots_.size(); ++i) {
    WmOverviewSnapshot* snapshot = snapshots_[i];
    if (!snapshot->IsVisible())
      snapshot->Show();
  }
}

void BrowserListener::SelectTab(int index, uint32 timestamp) {
  uint32 old_value = select_tab_timestamp_;
  select_tab_timestamp_ = timestamp;
  browser_->SelectTabContentsAt(index, true);
  select_tab_timestamp_ = old_value;
}

void BrowserListener::ConfigureCell(WmOverviewSnapshot* cell,
                                    TabContents* contents) {
  if (contents) {
    ThumbnailGenerator* generator =
        g_browser_process->GetThumbnailGenerator();
    // TODO: Make sure that if the cell gets deleted before the
    // callback is called that it sticks around until it gets
    // called.  (some kind of "in flight" list that uses linked_ptr
    // to make sure they don't actually get deleted?)  Also, make
    // sure that any request for a thumbnail eventually returns
    // (even if it has bogus data), so we don't leak orphaned cells,
    // which could happen if a tab is closed while it is being
    // rendered.
    ThumbnailGenerator::ThumbnailReadyCallback* callback =
        NewCallback(cell, &WmOverviewSnapshot::SetImage);
    gfx::Size cell_size = cell->size();

    // Ask for the page size to be twice the requested size of the
    // snapshot.
    generator->AskForSnapshot(contents->render_view_host(),
                              false,
                              callback,
                              gfx::Size(cell_size.width() * 2,
                                        cell_size.height() * 2),
                              cell_size);
  } else {
    // This happens because the contents haven't been loaded yet.

    // Make sure we set the snapshot image to something, otherwise
    // configured_snapshot remains false and
    // ConfigureNextUnconfiguredSnapshot would get stuck.
    cell->SetImage(SkBitmap());
  }
}

void BrowserListener::InsertSnapshot(int index) {
  WmOverviewSnapshot* snapshot = new WmOverviewSnapshot;
  gfx::Rect bounds =
      static_cast<BrowserView*>(browser_->window())->GetClientAreaBounds();
  gfx::Size size(bounds.width() / 2, bounds.height() / 2);
  snapshot->Init(size, browser_, index);
  snapshots_.insert(snapshots_.begin() + index, snapshot);
  snapshot->reload_snapshot();
  controller_->StartDelayTimer();
}

// Removes the snapshot at index.
void BrowserListener::ClearSnapshot(int index) {
  snapshots_[index]->CloseNow();
  snapshots_.erase(snapshots_.begin() + index);
}

void BrowserListener::RenumberSnapshots(int start_index) {
  for (SnapshotVector::size_type i = start_index; i < snapshots_.size(); ++i) {
    if (snapshots_[i]->index() != static_cast<int>(i))
      snapshots_[i]->UpdateIndex(browser_, i);
  }
}

///////////////////////////////////
// WmOverviewController methods

// static
WmOverviewController* WmOverviewController::instance() {
  static WmOverviewController* instance = NULL;
  if (!instance) {
    instance = Singleton<WmOverviewController>::get();
  }
  return instance;
}

WmOverviewController::WmOverviewController()
    : layout_mode_(ACTIVE_MODE) {
  AddAllBrowsers();

  if (registrar_.IsEmpty()) {
    // Make sure we get notifications for when the tab contents are
    // connected, so we know when a new browser has been created.
    registrar_.Add(this,
                   NotificationType::TAB_CONTENTS_CONNECTED,
                   NotificationService::AllSources());

    // Ask for notification when the snapshot source image has changed
    // and needs to be refreshed.
    registrar_.Add(this,
                   NotificationType::THUMBNAIL_GENERATOR_SNAPSHOT_CHANGED,
                   NotificationService::AllSources());
  }

  BrowserList::AddObserver(this);
  WmMessageListener::instance()->AddObserver(this);
}

WmOverviewController::~WmOverviewController() {
  WmMessageListener::instance()->RemoveObserver(this);
  BrowserList::RemoveObserver(this);
  listeners_.clear();
}

void WmOverviewController::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  switch (type.value) {
    // Now that the tab contents are ready, we create the listeners
    // and snapshots for any new browsers out there.  This actually
    // results in us traversing the list of browsers more often than
    // necessary (whenever a tab is connected, as opposed to only when
    // a new browser is created), but other notifications aren't
    // sufficient to know when the first tab of a new browser has its
    // dimensions set.  The implementation of AddAllBrowsers avoids
    // doing anything to already-existing browsers, so it's not a huge
    // problem, but still, it would be nice if there were a more
    // appropriate (browser-level) notification.
    case NotificationType::TAB_CONTENTS_CONNECTED:
      AddAllBrowsers();
      break;

    case NotificationType::THUMBNAIL_GENERATOR_SNAPSHOT_CHANGED: {
      // Don't do any dynamic updating if we're not in overview mode.
      if (layout_mode_ == OVERVIEW_MODE) {
        RenderWidgetHost* renderer = Details<RenderViewHost>(details).ptr();
        SnapshotImageChanged(renderer);
      }
      break;
    }
    default:
      // Do nothing.
      break;
  }
}

void WmOverviewController::SnapshotImageChanged(RenderWidgetHost* renderer) {
  // Find out which TabContents this renderer is attached to, and then
  // invalidate the associated snapshot so it'll update.
  BrowserListenerVector::iterator iter = listeners_.begin();
  while (iter != listeners_.end()) {
    for (int i = 0; i < (*iter)->count(); i++) {
      RenderWidgetHostView* view =
          (*iter)->GetTabContentsAt(i)->GetRenderWidgetHostView();
      if (view && view->GetRenderWidgetHost() == renderer) {
        (*iter)->ReloadSnapshot(i);
        return;
      }
    }
    ++iter;
  }
}

// Called immediately before a browser is removed from the list.
void WmOverviewController::OnBrowserRemoving(const Browser* browser) {
  for (BrowserListenerVector::iterator i = listeners_.begin();
       i != listeners_.end(); ++i) {
    if ((*i)->browser() == browser) {
      listeners_.erase(i);
      return;
    }
  }
}

BrowserView* GetBrowserViewForGdkWindow(GdkWindow* gdk_window) {
  gpointer data = NULL;
  gdk_window_get_user_data(gdk_window, &data);
  GtkWidget* widget = reinterpret_cast<GtkWidget*>(data);
  if (widget) {
    GtkWindow* gtk_window = GTK_WINDOW(widget);
    return BrowserView::GetBrowserViewForNativeWindow(gtk_window);
  } else {
    return NULL;
  }
}

void WmOverviewController::ProcessWmMessage(const WmIpc::Message& message,
                                            GdkWindow* window) {
  switch (message.type()) {
    case WM_IPC_MESSAGE_CHROME_NOTIFY_LAYOUT_MODE: {
      layout_mode_ = message.param(0) == 0 ? ACTIVE_MODE : OVERVIEW_MODE;
      if (layout_mode_ == ACTIVE_MODE || BrowserList::size() == 0) {
        Hide(message.param(1) != 0);
      } else {
        Show();
      }
      break;
    }
    case WM_IPC_MESSAGE_CHROME_NOTIFY_TAB_SELECT: {
      BrowserView* browser_window = GetBrowserViewForGdkWindow(window);
      // Find out which listener this goes to, and send it there.
      for (BrowserListenerVector::iterator i = listeners_.begin();
           i != listeners_.end(); ++i) {
        if ((*i)->browser()->window() == browser_window) {
          // param(0): index of the tab to select.
          // param(1): timestamp of the event.
          (*i)->SelectTab(message.param(0), message.param(1));
          break;
        }
      }
      break;
    }
    default:
      break;
  }
}

void WmOverviewController::StartDelayTimer() {
  // We leave the delay timer running if it already is -- this means
  // we're rate limiting the number of times we can reconfigure the
  // snapshots.  If we were to restart the delay timer, it could
  // result in a very long delay until they get configured if tabs
  // keep changing.
  if (layout_mode_ == OVERVIEW_MODE &&
      !delay_timer_.IsRunning() && !configure_timer_.IsRunning()) {
    delay_timer_.Start(
        base::TimeDelta::FromMilliseconds(kDelayTimeMs), this,
        &WmOverviewController::StartConfiguring);
  }
}

void WmOverviewController::RestoreTabSelections() {
  for (BrowserListenerVector::iterator i = listeners_.begin();
       i != listeners_.end(); ++i) {
    (*i)->RestoreOriginalSelectedTab();
  }
}

void WmOverviewController::SaveTabSelections() {
  for (BrowserListenerVector::iterator i = listeners_.begin();
       i != listeners_.end(); ++i) {
    (*i)->SaveCurrentTab();
  }
}

void WmOverviewController::Show() {
  SaveTabSelections();

  // Reset the timers.
  configure_timer_.Stop();
  delay_timer_.Stop();

  for (BrowserListenerVector::iterator i = listeners_.begin();
       i != listeners_.end(); ++i) {
    (*i)->ShowSnapshots();
  }
  StartDelayTimer();
}

void WmOverviewController::Hide(bool cancelled) {
  configure_timer_.Stop();
  delay_timer_.Stop();
  if (cancelled) {
    RestoreTabSelections();
  }
}

void WmOverviewController::StartConfiguring() {
  configure_timer_.Stop();
  configure_timer_.Start(
      base::TimeDelta::FromMilliseconds(kConfigureTimeMs), this,
      &WmOverviewController::ConfigureNextUnconfiguredSnapshot);
}

// Just configure one unconfigured cell.  If there aren't any left,
// then stop the timer.
void WmOverviewController::ConfigureNextUnconfiguredSnapshot() {
  for (BrowserListenerVector::size_type i = 0; i < listeners_.size(); ++i) {
    BrowserListener* listener = listeners_[i].get();
    if (listener->ConfigureNextUnconfiguredSnapshot())
      return;
  }
  configure_timer_.Stop();
}

void WmOverviewController::AddAllBrowsers() {
  // Make a copy so the old ones aren't deleted yet.
  BrowserListenerVector old_listeners;

  listeners_.swap(old_listeners);

  // Iterator through the browser list, adding all the browsers in the
  // new order.  If they were in the old list of listeners, just copy
  // that linked pointer, instead of making a new listener, so that we
  // can avoid lots of spurious destroy/create messages.
  BrowserList::const_iterator iterator = BrowserList::begin();
  while (iterator != BrowserList::end()) {
    // Don't add a browser to the list if that type of browser doesn't
    // have snapshots in overview mode.
    if ((*iterator)->type() != Browser::TYPE_NORMAL &&
        (*iterator)->type() != Browser::TYPE_APP &&
        (*iterator)->type() != Browser::TYPE_EXTENSION_APP) {
      ++iterator;
      continue;
    }

    BrowserListenerVector::value_type item(
        BrowserListenerVector::value_type(NULL));
    for (BrowserListenerVector::iterator old_iter = old_listeners.begin();
         old_iter != old_listeners.end(); ++old_iter) {
      if ((*old_iter)->browser() == *iterator) {
        item = *old_iter;
        break;
      }
    }

    // This browser isn't tracked by any listener, so create it.
    if (item.get() == NULL) {
      item = BrowserListenerVector::value_type(
          new BrowserListener(*iterator, this));
    }
    listeners_.push_back(item);
    ++iterator;
  }
}

}  // namespace chromeos
