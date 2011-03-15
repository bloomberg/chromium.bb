// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/wm_overview_controller.h"

#include <algorithm>
#include <vector>

#include "base/linked_ptr.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/browser/chromeos/wm_overview_fav_icon.h"
#include "chrome/browser/chromeos/wm_overview_snapshot.h"
#include "chrome/browser/chromeos/wm_overview_title.h"
#include "chrome/browser/tab_contents/thumbnail_generator.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host.h"
#include "content/browser/renderer_host/render_widget_host_view.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/browser/tab_contents/tab_contents_view.h"
#include "content/common/notification_service.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"
#include "views/window/window.h"

using std::vector;

#if !defined(OS_CHROMEOS)
#error This file is only meant to be compiled for ChromeOS
#endif

namespace chromeos {

// Use this timer to delay consecutive snapshots during the updating process.
// We will start the timer upon successfully retrieving a new snapshot, or if
// for some reason the current snapshot is still pending (while Chrome is
// still loading the page.)
static const int kDelayTimeMs = 10;

// This is the size of the web page when we lay it out for a snapshot.
static const int kSnapshotWebPageWidth = 1024;
static const int kSnapshotWebPageHeight = 1280;
static const double kSnapshotWebPageRatio =
    static_cast<double>(kSnapshotWebPageWidth) / kSnapshotWebPageHeight;

// This is the maximum percentage of the original browser client area
// that a snapshot can take up.
static const double kSnapshotMaxSizeRatio = 0.77;

// This is the height of the title in pixels.
static const int kTitleHeight = 32;

// The number of additional padding pixels to remove from the title width.
static const int kFavIconPadding = 5;

class BrowserListener : public TabStripModelObserver {
 public:
  BrowserListener(Browser* browser, WmOverviewController* parent);
  ~BrowserListener();

  // Begin TabStripModelObserver methods
  virtual void TabInsertedAt(TabContentsWrapper* contents,
                             int index,
                             bool foreground);
  virtual void TabClosingAt(TabStripModel* tab_strip_model,
                            TabContentsWrapper* contents,
                            int index) {}
  virtual void TabDetachedAt(TabContentsWrapper* contents, int index);
  virtual void TabMoved(TabContentsWrapper* contents,
                        int from_index,
                        int to_index);
  virtual void TabChangedAt(TabContentsWrapper* contents, int index,
                            TabStripModelObserver::TabChangeType change_type);
  virtual void TabStripEmpty();
  virtual void TabDeselected(TabContentsWrapper* contents) {}
  virtual void TabSelectedAt(TabContentsWrapper* old_contents,
                             TabContentsWrapper* new_contents,
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
  void MarkSnapshotAsDirty(int index);

  // Updates the selected index and tab count on the toplevel window.
  void UpdateSelectedIndex(int index);

  // Update the first "dirty" snapshot, which is ordered after (excluding)
  // the snapshot whose index is given by |start_from|; When |start_from| is
  // -1, search start at the begining of the list.
  // Return the index of the snapshot which is actually updated; -1 if there
  // are no more tab contents (after |start_from|) to configure on this
  // listener.
  int ConfigureNextUnconfiguredSnapshot(int start_from);

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

  // Callback for |AskForSnapshot|, start delay timer for next round.
  void OnSnapshotReady(const SkBitmap& sk_bitmap);

  // Returns the tab contents from the tab model for this child at index.
  TabContents* GetTabContentsAt(int index) const  {
    return browser_->tabstrip_model()->GetTabContentsAt(index)->tab_contents();
  }

 private:
  // Calculate the size of a cell based on the browser window's size.
  gfx::Size CalculateCellSize();

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

  // Which renderer host we are working on.
  RenderWidgetHost* current_renderer_host_; // Not owned

  // Widgets containing snapshot images for this browser.  Note that
  // these are all subclasses of WidgetGtk, and they are all added to
  // parents, so they will be deleted by the parents when they are
  // closed.
  struct SnapshotNode {
    WmOverviewSnapshot* snapshot;  // Not owned
    WmOverviewTitle* title;  // Not owned
    WmOverviewFavIcon* fav_icon;  // Not owned
  };
  typedef std::vector<SnapshotNode> SnapshotVector;
  SnapshotVector snapshots_;

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
      current_renderer_host_(NULL),
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

void BrowserListener::TabInsertedAt(TabContentsWrapper* contents,
                                    int index,
                                    bool foreground) {
  InsertSnapshot(index);
  RenumberSnapshots(index);
  UpdateSelectedIndex(browser_->selected_index());
}

void BrowserListener::TabDetachedAt(TabContentsWrapper* contents, int index) {
  ClearSnapshot(index);
  UpdateSelectedIndex(browser_->selected_index());
  RenumberSnapshots(index);
}

void BrowserListener::TabMoved(TabContentsWrapper* contents,
                               int from_index,
                               int to_index) {
  // Need to reorder tab in the snapshots list, and reset the window
  // type atom on the affected snapshots (the one moved, and all the
  // ones after it), so that their indices are correct.
  SnapshotNode node = snapshots_[from_index];
  snapshots_.erase(snapshots_.begin() + from_index);
  snapshots_.insert(snapshots_.begin() + to_index, node);

  RenumberSnapshots(std::min(to_index, from_index));
  UpdateSelectedIndex(browser_->selected_index());
}

void BrowserListener::TabChangedAt(
    TabContentsWrapper* contents,
    int index,
    TabStripModelObserver::TabChangeType change_type) {
  if (change_type != TabStripModelObserver::LOADING_ONLY) {
    snapshots_[index].title->SetTitle(contents->tab_contents()->GetTitle());
    snapshots_[index].title->SetUrl(contents->tab_contents()->GetURL());
    snapshots_[index].fav_icon->SetFavicon(
        contents->tab_contents()->GetFavicon());
    if (change_type != TabStripModelObserver::TITLE_NOT_LOADING)
      MarkSnapshotAsDirty(index);
  }
}

void BrowserListener::TabStripEmpty() {
  snapshots_.clear();
}

void BrowserListener::TabSelectedAt(TabContentsWrapper* old_contents,
                                    TabContentsWrapper* new_contents,
                                    int index,
                                    bool user_gesture) {
  if (old_contents == new_contents)
    return;

  UpdateSelectedIndex(index);
}

void BrowserListener::MarkSnapshotAsDirty(int index) {
  snapshots_[index].snapshot->reload_snapshot();
  controller_->UpdateSnapshots();
}

void BrowserListener::RecreateSnapshots() {
  snapshots_.clear();

  for (int i = 0; i < count(); ++i)
    InsertSnapshot(i);

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

int BrowserListener::ConfigureNextUnconfiguredSnapshot(int start_from) {
  for (SnapshotVector::size_type i = start_from + 1;
      i < snapshots_.size(); ++i) {
    WmOverviewSnapshot* cell = snapshots_[i].snapshot;
    if (!cell->configured_snapshot()) {
      ConfigureCell(cell, i);
      return i;
    }
  }
  return -1;
}

void BrowserListener::RestoreOriginalSelectedTab() {
  if (original_selected_tab_ >= 0) {
    browser_->SelectTabContentsAt(original_selected_tab_, false);
    UpdateSelectedIndex(browser_->selected_index());
  }
}

void BrowserListener::ShowSnapshots() {
  for (SnapshotVector::size_type i = 0; i < snapshots_.size(); ++i) {
    const SnapshotNode& node = snapshots_[i];
    if (!node.snapshot->IsVisible())
      node.snapshot->Show();
    if (!snapshots_[i].title->IsVisible())
      node.title->Show();
    if (!snapshots_[i].fav_icon->IsVisible())
      node.fav_icon->Show();
  }
}

void BrowserListener::SelectTab(int index, uint32 timestamp) {
  // Ignore requests to switch to non-existent tabs (the window manager gets
  // notified asynchronously about the number of tabs in each window, so there's
  // no guarantee that the messages that it sends us will make sense).
  if (index < 0 || index >= browser_->tab_count())
    return;

  uint32 old_value = select_tab_timestamp_;
  select_tab_timestamp_ = timestamp;
  browser_->SelectTabContentsAt(index, true);
  select_tab_timestamp_ = old_value;
}

gfx::Size BrowserListener::CalculateCellSize() {
  // Make the page size and the cell size a fixed size for overview
  // mode.  The cell size is calculated based on the desired maximum
  // size on the screen, so it's related to the width and height of
  // the browser client area.  In this way, when this snapshot gets
  // to the window manager, it will already have the correct size,
  // and will be scaled by 1.0, meaning that it won't be resampled
  // and will not be blurry.
  gfx::Rect bounds = static_cast<BrowserView*>(browser_->window())->
                     GetClientAreaBounds();
  const gfx::Size max_size = gfx::Size(
      bounds.width() * kSnapshotMaxSizeRatio,
      bounds.height() * kSnapshotMaxSizeRatio);
  const double max_size_ratio = static_cast<double>(max_size.width()) /
                                max_size.height();
  gfx::Size cell_size;
  if (kSnapshotWebPageRatio > max_size_ratio) {
    const double scale_factor =
        static_cast<double>(max_size.width())/ kSnapshotWebPageWidth;
    cell_size = gfx::Size(max_size.width(),
                          kSnapshotWebPageHeight * scale_factor + 0.5);
  } else {
    const double scale_factor =
        static_cast<double>(max_size.height())/ kSnapshotWebPageHeight;
    cell_size = gfx::Size(kSnapshotWebPageWidth * scale_factor + 0.5,
                          max_size.height());
  }
  return cell_size;
}

void BrowserListener::OnSnapshotReady(const SkBitmap& sk_bitmap) {
  for (int i = 0; i < count(); i++) {
    RenderWidgetHostView* view =
        GetTabContentsAt(i)->GetRenderWidgetHostView();
    if (view && view->GetRenderWidgetHost() == current_renderer_host_) {
      snapshots_[i].snapshot->SetImage(sk_bitmap);
      current_renderer_host_ = NULL;

      // Start timer for next round of snapshot updating.
      controller_->StartDelayTimer();
      return;
    }
  }
  DCHECK(current_renderer_host_ == NULL);
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
        NewCallback(this, &BrowserListener::OnSnapshotReady);

    current_renderer_host_ = contents->render_view_host();
    generator->AskForSnapshot(contents->render_view_host(),
                              false,
                              callback,
                              gfx::Size(kSnapshotWebPageWidth,
                                        kSnapshotWebPageHeight),
                              CalculateCellSize());
  } else {
    // This happens because the contents haven't been loaded yet.

    // Make sure we set the snapshot image to something, otherwise
    // configured_snapshot remains false and
    // ConfigureNextUnconfiguredSnapshot would get stuck.
    current_renderer_host_ = NULL;
    cell->SetImage(SkBitmap());
    cell->reload_snapshot();
    controller_->StartDelayTimer();
  }
}

void BrowserListener::InsertSnapshot(int index) {
  SnapshotNode node;
  node.snapshot = new WmOverviewSnapshot;
  gfx::Size cell_size = CalculateCellSize();
  node.snapshot->Init(cell_size, browser_, index);

  node.fav_icon = new WmOverviewFavIcon;
  node.fav_icon->Init(node.snapshot);
  node.fav_icon->SetFavicon(browser_->GetTabContentsAt(index)->GetFavicon());

  node.title = new WmOverviewTitle;
  node.title->Init(gfx::Size(std::max(0, cell_size.width() -
                                      WmOverviewFavIcon::kIconSize -
                                      kFavIconPadding),
                             kTitleHeight), node.snapshot);
  node.title->SetTitle(browser_->GetTabContentsAt(index)->GetTitle());

  snapshots_.insert(snapshots_.begin() + index, node);
  node.snapshot->reload_snapshot();
  controller_->UpdateSnapshots();
}

// Removes the snapshot at index.
void BrowserListener::ClearSnapshot(int index) {
  snapshots_[index].snapshot->CloseNow();
  snapshots_[index].title->CloseNow();
  snapshots_[index].fav_icon->CloseNow();
  snapshots_.erase(snapshots_.begin() + index);
}

void BrowserListener::RenumberSnapshots(int start_index) {
  for (SnapshotVector::size_type i = start_index; i < snapshots_.size(); ++i) {
    if (snapshots_[i].snapshot->index() != static_cast<int>(i))
      snapshots_[i].snapshot->UpdateIndex(browser_, i);
  }
}

///////////////////////////////////
// WmOverviewController methods

// static
WmOverviewController* WmOverviewController::GetInstance() {
  static WmOverviewController* instance = NULL;
  if (!instance) {
    instance = Singleton<WmOverviewController>::get();
  }
  return instance;
}

WmOverviewController::WmOverviewController()
    : layout_mode_(ACTIVE_MODE),
      updating_snapshots_(false),
      browser_listener_index_(0),
      tab_contents_index_(-1) {
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
  WmMessageListener::GetInstance()->AddObserver(this);
}

WmOverviewController::~WmOverviewController() {
  WmMessageListener::GetInstance()->RemoveObserver(this);
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
      // It's OK to do this in active mode too -- nothing will happen
      // except invalidation of the snapshot, because the delay timer
      // won't start until we're in overview mode.
      RenderWidgetHost* renderer = Details<RenderViewHost>(details).ptr();
      SnapshotImageChanged(renderer);
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
        (*iter)->MarkSnapshotAsDirty(i);
        return;
      }
    }
    ++iter;
  }
  DLOG(ERROR) << "SnapshotImageChanged, but we do not know which it is?";
}

void WmOverviewController::OnBrowserRemoved(const Browser* browser) {
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
  // We're rate limiting the number of times we can reconfigure the
  // snapshots.  If we were to restart the delay timer, it could
  // result in a very long delay until they get configured if tabs
  // keep changing.
  updating_snapshots_ = false;
  if (layout_mode_ == OVERVIEW_MODE) {
    delay_timer_.Start(
        base::TimeDelta::FromMilliseconds(kDelayTimeMs), this,
        &WmOverviewController::UpdateSnapshots);
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

  for (BrowserListenerVector::iterator i = listeners_.begin();
       i != listeners_.end(); ++i) {
    (*i)->ShowSnapshots();
  }

  // TODO(jiesun): Make the focused tab as the start point.
  browser_listener_index_ = 0;
  tab_contents_index_ = -1;
  UpdateSnapshots();
}

void WmOverviewController::Hide(bool cancelled) {
  delay_timer_.Stop();
  updating_snapshots_ = false;
  if (cancelled) {
    RestoreTabSelections();
  }
}

void WmOverviewController::UpdateSnapshots() {

  // Only updating snapshots during overview mode.
  if (layout_mode_ != OVERVIEW_MODE)
    return;

  // Only reloading snapshots when not already started.
  if (updating_snapshots_ || delay_timer_.IsRunning())
    return;

  // Only update one snapshot in round-robin mode.
  // Start delay timer after each update unless all snapshots had been updated.
  if (!listeners_.size())
    return;

  if (int(listeners_.size()) <= browser_listener_index_) {
    DLOG(INFO) << "Browser listener(s) have disappeared since last update";
    browser_listener_index_ = 0;
    tab_contents_index_ = -1;
  } else {
    BrowserListener* listener = listeners_[browser_listener_index_].get();
    if (listener->count() <= tab_contents_index_) {
      DLOG(INFO) << "Tab content(s) have disappeared since last update";
      tab_contents_index_ = -1;
    }
  }

  int browser_listener_index = browser_listener_index_;
  int tab_contents_index = tab_contents_index_;

  bool loop_back = false;
  while (1) {
    BrowserListener* listener = listeners_[browser_listener_index].get();
    tab_contents_index =
        listener->ConfigureNextUnconfiguredSnapshot(tab_contents_index);
    if (tab_contents_index >= 0) {
      updating_snapshots_ = true;  // Prevent future parallel updates.
      browser_listener_index_ = browser_listener_index;
      tab_contents_index_ = tab_contents_index;
      return;
    }

    // Found next one;
    browser_listener_index++;
    browser_listener_index = browser_listener_index % listeners_.size();
    tab_contents_index = -1;

    if (loop_back)
      break;
    loop_back = browser_listener_index == browser_listener_index_;
  }

  // All snapshots have been fully updated.
  updating_snapshots_ = false;
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
        (*iterator)->type() != Browser::TYPE_APP) {
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
