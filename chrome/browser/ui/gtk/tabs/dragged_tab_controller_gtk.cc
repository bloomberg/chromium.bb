// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/tabs/dragged_tab_controller_gtk.h"

#include <algorithm>

#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/app_modal_dialogs/message_box_handler.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/tabs/dragged_view_gtk.h"
#include "chrome/browser/ui/gtk/tabs/tab_strip_gtk.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "ui/gfx/screen.h"

namespace {

// Delay, in ms, during dragging before we bring a window to front.
const int kBringToFrontDelay = 750;

// Used to determine how far a tab must obscure another tab in order to swap
// their indexes.
const int kHorizontalMoveThreshold = 16;  // pixels

// How far a drag must pull a tab out of the tabstrip in order to detach it.
const int kVerticalDetachMagnetism = 15;  // pixels

// Returns the bounds that will be used for insertion index calculation.
// |bounds| is the actual tab bounds, but subtracting the overlapping areas from
// both sides makes the calculations much simpler.
gfx::Rect GetEffectiveBounds(const gfx::Rect& bounds) {
  gfx::Rect effective_bounds(bounds);
  effective_bounds.set_width(effective_bounds.width() - 2 * 16);
  effective_bounds.set_x(effective_bounds.x() + 16);
  return effective_bounds;
}

}  // namespace

DraggedTabControllerGtk::DraggedTabControllerGtk(
    TabStripGtk* source_tabstrip,
    TabGtk* source_tab,
    const std::vector<TabGtk*>& tabs)
    : source_tabstrip_(source_tabstrip),
      attached_tabstrip_(source_tabstrip),
      in_destructor_(false),
      last_move_screen_x_(0),
      initial_move_(true) {
  DCHECK(!tabs.empty());
  DCHECK(std::find(tabs.begin(), tabs.end(), source_tab) != tabs.end());

  std::vector<DraggedTabData> drag_data;
  for (size_t i = 0; i < tabs.size(); i++)
    drag_data.push_back(InitDraggedTabData(tabs[i]));

  int source_tab_index =
      std::find(tabs.begin(), tabs.end(), source_tab) - tabs.begin();
  drag_data_.reset(new DragData(drag_data, source_tab_index));
}

DraggedTabControllerGtk::~DraggedTabControllerGtk() {
  in_destructor_ = true;
  // Need to delete the dragged tab here manually _before_ we reset the dragged
  // contents to NULL, otherwise if the view is animating to its destination
  // bounds, it won't be able to clean up properly since its cleanup routine
  // uses GetIndexForDraggedContents, which will be invalid.
  CleanUpDraggedTabs();
  dragged_view_.reset();
  drag_data_.reset();
}

void DraggedTabControllerGtk::CaptureDragInfo(const gfx::Point& mouse_offset) {
  start_screen_point_ = gfx::Screen::GetCursorScreenPoint();
  mouse_offset_ = mouse_offset;
}

void DraggedTabControllerGtk::Drag() {
  if (!drag_data_->GetSourceTabData()->tab_ ||
      !drag_data_->GetSourceTabContentsWrapper()) {
    return;
  }

  bring_to_front_timer_.Stop();

  EnsureDraggedView();

  // Before we get to dragging anywhere, ensure that we consider ourselves
  // attached to the source tabstrip.
  if (drag_data_->GetSourceTabData()->tab_->IsVisible()) {
    Attach(source_tabstrip_, gfx::Point());
  }

  if (!drag_data_->GetSourceTabData()->tab_->IsVisible()) {
    // TODO(jhawkins): Save focus.
    ContinueDragging();
  }
}

bool DraggedTabControllerGtk::EndDrag(bool canceled) {
  return EndDragImpl(canceled ? CANCELED : NORMAL);
}

TabGtk* DraggedTabControllerGtk::GetDraggedTabForContents(
    TabContents* contents) {
  if (attached_tabstrip_ == source_tabstrip_) {
    for (size_t i = 0; i < drag_data_->size(); i++) {
      if (contents == drag_data_->get(i)->contents_->tab_contents())
        return drag_data_->get(i)->tab_;
    }
  }
  return NULL;
}

bool DraggedTabControllerGtk::IsDraggingTab(const TabGtk* tab) {
  for (size_t i = 0; i < drag_data_->size(); i++) {
    if (tab == drag_data_->get(i)->tab_)
      return true;
  }
  return false;
}

bool DraggedTabControllerGtk::IsDraggingTabContents(
    const TabContentsWrapper* tab_contents) {
  for (size_t i = 0; i < drag_data_->size(); i++) {
    if (tab_contents == drag_data_->get(i)->contents_)
      return true;
  }
  return false;
}

bool DraggedTabControllerGtk::IsTabDetached(const TabGtk* tab) {
  return IsDraggingTab(tab) && attached_tabstrip_ == NULL;
}

DraggedTabData DraggedTabControllerGtk::InitDraggedTabData(TabGtk* tab) {
  int source_model_index = source_tabstrip_->GetIndexOfTab(tab);
  TabContentsWrapper* contents =
      source_tabstrip_->model()->GetTabContentsAt(source_model_index);
  bool pinned = source_tabstrip_->IsTabPinned(tab);
  bool mini = source_tabstrip_->model()->IsMiniTab(source_model_index);
  // We need to be the delegate so we receive messages about stuff,
  // otherwise our dragged_contents() may be replaced and subsequently
  // collected/destroyed while the drag is in process, leading to
  // nasty crashes.
  TabContentsDelegate* original_delegate =
      contents->tab_contents()->delegate();
  contents->tab_contents()->set_delegate(this);

  DraggedTabData dragged_tab_data(tab, contents, original_delegate,
                                  source_model_index, pinned, mini);
  registrar_.Add(
      this,
      content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
      content::Source<TabContents>(dragged_tab_data.contents_->tab_contents()));
  return dragged_tab_data;
}

////////////////////////////////////////////////////////////////////////////////
// DraggedTabControllerGtk, TabContentsDelegate implementation:

// TODO(adriansc): Remove this method once refactoring has changed all call
// sites.
TabContents* DraggedTabControllerGtk::OpenURLFromTab(
    TabContents* source,
    const GURL& url,
    const GURL& referrer,
    WindowOpenDisposition disposition,
    content::PageTransition transition) {
  return OpenURLFromTab(source,
                        OpenURLParams(url, referrer, disposition, transition,
                                      false));
}

TabContents* DraggedTabControllerGtk::OpenURLFromTab(
    TabContents* source,
    const OpenURLParams& params) {
  if (drag_data_->GetSourceTabData()->original_delegate_) {
    OpenURLParams forward_params = params;
    if (params.disposition == CURRENT_TAB)
      forward_params.disposition = NEW_WINDOW;

    return drag_data_->GetSourceTabData()->original_delegate_->
        OpenURLFromTab(source, forward_params);
  }
  return NULL;
}

void DraggedTabControllerGtk::NavigationStateChanged(const TabContents* source,
                                                     unsigned changed_flags) {
  if (dragged_view_.get())
    dragged_view_->Update();
}

void DraggedTabControllerGtk::AddNewContents(TabContents* source,
                                             TabContents* new_contents,
                                             WindowOpenDisposition disposition,
                                             const gfx::Rect& initial_pos,
                                             bool user_gesture) {
  DCHECK(disposition != CURRENT_TAB);

  // Theoretically could be called while dragging if the page tries to
  // spawn a window. Route this message back to the browser in most cases.
  if (drag_data_->GetSourceTabData()->original_delegate_) {
    drag_data_->GetSourceTabData()->original_delegate_->AddNewContents(
        source, new_contents, disposition, initial_pos, user_gesture);
  }
}

void DraggedTabControllerGtk::LoadingStateChanged(TabContents* source) {
  // TODO(jhawkins): It would be nice to respond to this message by changing the
  // screen shot in the dragged tab.
  if (dragged_view_.get())
    dragged_view_->Update();
}

content::JavaScriptDialogCreator*
DraggedTabControllerGtk::GetJavaScriptDialogCreator() {
  return GetJavaScriptDialogCreatorInstance();
}

////////////////////////////////////////////////////////////////////////////////
// DraggedTabControllerGtk, content::NotificationObserver implementation:

void DraggedTabControllerGtk::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(type == content::NOTIFICATION_TAB_CONTENTS_DESTROYED);
  TabContents* destroyed_contents = content::Source<TabContents>(source).ptr();
  for (size_t i = 0; i < drag_data_->size(); ++i) {
    if (drag_data_->get(i)->contents_->tab_contents() == destroyed_contents) {
      // One of the tabs we're dragging has been destroyed. Cancel the drag.
      if (destroyed_contents->delegate() == this)
        destroyed_contents->set_delegate(NULL);
      drag_data_->get(i)->contents_ = NULL;
      drag_data_->get(i)->original_delegate_ = NULL;
      EndDragImpl(TAB_DESTROYED);
      return;
    }
  }
  // If we get here it means we got notification for a tab we don't know about.
  NOTREACHED();
}

gfx::Point DraggedTabControllerGtk::GetWindowCreatePoint() const {
  gfx::Point creation_point = gfx::Screen::GetCursorScreenPoint();
  gfx::Point distance_from_origin =
      dragged_view_->GetDistanceFromTabStripOriginToMousePointer();
  // TODO(dpapad): offset also because of tabstrip origin being different than
  // window origin.
  creation_point.Offset(-distance_from_origin.x(), -distance_from_origin.y());
  return creation_point;
}

void DraggedTabControllerGtk::ContinueDragging() {
  // TODO(jhawkins): We don't handle the situation where the last tab is dragged
  // out of a window, so we'll just go with the way Windows handles dragging for
  // now.
  gfx::Point screen_point = gfx::Screen::GetCursorScreenPoint();

  // Determine whether or not we have dragged over a compatible TabStrip in
  // another browser window. If we have, we should attach to it and start
  // dragging within it.
#if defined(OS_CHROMEOS)
  // We don't allow detaching on chrome os.
  TabStripGtk* target_tabstrip = source_tabstrip_;
#else
  TabStripGtk* target_tabstrip = GetTabStripForPoint(screen_point);
#endif
  if (target_tabstrip != attached_tabstrip_) {
    // Make sure we're fully detached from whatever TabStrip we're attached to
    // (if any).
    if (attached_tabstrip_)
      Detach();

    if (target_tabstrip)
      Attach(target_tabstrip, screen_point);
  }

  if (!target_tabstrip) {
    bring_to_front_timer_.Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(kBringToFrontDelay), this,
        &DraggedTabControllerGtk::BringWindowUnderMouseToFront);
  }

  if (attached_tabstrip_)
    MoveAttached(screen_point);
  else
    MoveDetached(screen_point);
}

void DraggedTabControllerGtk::RestoreSelection(TabStripModel* model) {
  for (size_t i = 0; i < drag_data_->size(); ++i) {
    int index = model->GetIndexOfTabContents(drag_data_->get(i)->contents_);
    model->AddTabAtToSelection(index);
  }
}

void DraggedTabControllerGtk::MoveAttached(const gfx::Point& screen_point) {
  DCHECK(attached_tabstrip_);

  gfx::Point dragged_view_point = GetDraggedViewPoint(screen_point);
  TabStripModel* attached_model = attached_tabstrip_->model();

  std::vector<TabGtk*> tabs(drag_data_->GetDraggedTabs());

  // Determine the horizontal move threshold. This is dependent on the width
  // of tabs. The smaller the tabs compared to the standard size, the smaller
  // the threshold.
  double unselected, selected;
  attached_tabstrip_->GetCurrentTabWidths(&unselected, &selected);
  double ratio = unselected / TabGtk::GetStandardSize().width();
  int threshold = static_cast<int>(ratio * kHorizontalMoveThreshold);

  // Update the model, moving the TabContents from one index to another. Do this
  // only if we have moved a minimum distance since the last reorder (to prevent
  // jitter) or if this is the first move and the tabs are not consecutive.
  if (abs(screen_point.x() - last_move_screen_x_) > threshold ||
      (initial_move_ && !AreTabsConsecutive())) {
    if (initial_move_ && !AreTabsConsecutive()) {
      // Making dragged tabs adjacent, this is done only once, if necessary.
      attached_tabstrip_->model()->MoveSelectedTabsTo(
          drag_data_->GetSourceTabData()->source_model_index_ -
              drag_data_->source_tab_index());
    }
    gfx::Rect bounds = GetDraggedViewTabStripBounds(dragged_view_point);
    int to_index = GetInsertionIndexForDraggedBounds(
        GetEffectiveBounds(bounds));
    to_index = NormalizeIndexToAttachedTabStrip(to_index);
    last_move_screen_x_ = screen_point.x();
    attached_model->MoveSelectedTabsTo(to_index);
  }

  dragged_view_->MoveAttachedTo(dragged_view_point);
  initial_move_ = false;
}

void DraggedTabControllerGtk::MoveDetached(const gfx::Point& screen_point) {
  DCHECK(!attached_tabstrip_);
  // Just moving the dragged view. There are no changes to the model if we're
  // detached.
  dragged_view_->MoveDetachedTo(screen_point);
}

TabStripGtk* DraggedTabControllerGtk::GetTabStripForPoint(
    const gfx::Point& screen_point) {
  GtkWidget* dragged_window = dragged_view_->widget();
  dock_windows_.insert(dragged_window);
  gfx::NativeWindow local_window =
      DockInfo::GetLocalProcessWindowAtPoint(screen_point, dock_windows_);
  dock_windows_.erase(dragged_window);
  if (!local_window)
    return NULL;

  BrowserWindowGtk* browser =
      BrowserWindowGtk::GetBrowserWindowForNativeWindow(local_window);
  if (!browser)
    return NULL;

  TabStripGtk* other_tabstrip = browser->tabstrip();
  if (!other_tabstrip->IsCompatibleWith(source_tabstrip_))
    return NULL;

  return GetTabStripIfItContains(other_tabstrip, screen_point);
}

TabStripGtk* DraggedTabControllerGtk::GetTabStripIfItContains(
    TabStripGtk* tabstrip, const gfx::Point& screen_point) const {
  // Make sure the specified screen point is actually within the bounds of the
  // specified tabstrip...
  gfx::Rect tabstrip_bounds =
      gtk_util::GetWidgetScreenBounds(tabstrip->tabstrip_.get());
  if (screen_point.x() < tabstrip_bounds.right() &&
      screen_point.x() >= tabstrip_bounds.x()) {
    // TODO(beng): make this be relative to the start position of the mouse for
    // the source TabStrip.
    int upper_threshold = tabstrip_bounds.bottom() + kVerticalDetachMagnetism;
    int lower_threshold = tabstrip_bounds.y() - kVerticalDetachMagnetism;
    if (screen_point.y() >= lower_threshold &&
        screen_point.y() <= upper_threshold) {
      return tabstrip;
    }
  }

  return NULL;
}

void DraggedTabControllerGtk::Attach(TabStripGtk* attached_tabstrip,
                                     const gfx::Point& screen_point) {
  attached_tabstrip_ = attached_tabstrip;
  attached_tabstrip_->GenerateIdealBounds();

  std::vector<TabGtk*> attached_dragged_tabs =
      GetTabsMatchingDraggedContents(attached_tabstrip_);

  // Update the tab first, so we can ask it for its bounds and determine
  // where to insert the hidden tab.

  // If this is the first time Attach is called for this drag, we're attaching
  // to the source tabstrip, and we should assume the tab count already
  // includes this tab since we haven't been detached yet. If we don't do this,
  // the dragged representation will be a different size to others in the
  // tabstrip.
  int tab_count = attached_tabstrip_->GetTabCount();
  int mini_tab_count = attached_tabstrip_->GetMiniTabCount();
  if (attached_dragged_tabs.size() == 0) {
    tab_count += drag_data_->size();
    mini_tab_count += drag_data_->mini_tab_count();
  }

  double unselected_width = 0, selected_width = 0;
  attached_tabstrip_->GetDesiredTabWidths(tab_count, mini_tab_count,
                                          &unselected_width, &selected_width);

  GtkWidget* parent_window = gtk_widget_get_parent(
      gtk_widget_get_parent(attached_tabstrip_->tabstrip_.get()));
  gfx::Rect window_bounds = gtk_util::GetWidgetScreenBounds(parent_window);
  dragged_view_->Attach(static_cast<int>(floor(selected_width + 0.5)),
                        TabGtk::GetMiniWidth(), window_bounds.width());

  if (attached_dragged_tabs.size() == 0) {
    // There is no tab in |attached_tabstrip| that corresponds to the dragged
    // TabContents. We must now create one.

    // Remove ourselves as the delegate now that the dragged TabContents is
    // being inserted back into a Browser.
    for (size_t i = 0; i < drag_data_->size(); ++i) {
      drag_data_->get(i)->contents_->tab_contents()->set_delegate(NULL);
      drag_data_->get(i)->original_delegate_ = NULL;
    }

    // Return the TabContents' to normalcy.
    drag_data_->GetSourceTabContents()->set_capturing_contents(false);

    // We need to ask the tabstrip we're attached to ensure that the ideal
    // bounds for all its tabs are correctly generated, because the calculation
    // in GetInsertionIndexForDraggedBounds needs them to be to figure out the
    // appropriate insertion index.
    attached_tabstrip_->GenerateIdealBounds();

    // Inserting counts as a move. We don't want the tabs to jitter when the
    // user moves the tab immediately after attaching it.
    last_move_screen_x_ = screen_point.x();

    // Figure out where to insert the tab based on the bounds of the dragged
    // representation and the ideal bounds of the other tabs already in the
    // strip. ("ideal bounds" are stable even if the tabs' actual bounds are
    // changing due to animation).
    gfx::Rect bounds =
        GetDraggedViewTabStripBounds(GetDraggedViewPoint(screen_point));
    int index = GetInsertionIndexForDraggedBounds(GetEffectiveBounds(bounds));
    for (size_t i = 0; i < drag_data_->size(); ++i) {
      attached_tabstrip_->model()->InsertTabContentsAt(
          index + i, drag_data_->get(i)->contents_,
          drag_data_->GetAddTypesForDraggedTabAt(i));
    }
    RestoreSelection(attached_tabstrip_->model());
    attached_dragged_tabs = GetTabsMatchingDraggedContents(attached_tabstrip_);
  }
  // We should now have a tab.
  DCHECK(attached_dragged_tabs.size() == drag_data_->size());
  SetDraggedTabsVisible(false, false);
  // TODO(jhawkins): Move the corresponding window to the front.
}

void DraggedTabControllerGtk::Detach() {
  // Update the Model.
  TabStripModel* attached_model = attached_tabstrip_->model();
  for (size_t i = 0; i < drag_data_->size(); ++i) {
    int index =
        attached_model->GetIndexOfTabContents(drag_data_->get(i)->contents_);
    if (index >= 0 && index < attached_model->count()) {
      // Sometimes, DetachTabContentsAt has consequences that result in
      // attached_tabstrip_ being set to NULL, so we need to save it first.
      attached_model->DetachTabContentsAt(index);
    }
  }

  // If we've removed the last tab from the tabstrip, hide the frame now.
  if (attached_model->empty())
    HideWindow();

  // Update the dragged tab. This NULL check is necessary apparently in some
  // conditions during automation where the view_ is destroyed inside a
  // function call preceding this point but after it is created.
  if (dragged_view_.get()) {
    dragged_view_->Detach();
  }

  // Detaching resets the delegate, but we still want to be the delegate.
  for (size_t i = 0; i < drag_data_->size(); ++i)
    drag_data_->get(i)->contents_->tab_contents()->set_delegate(this);

  attached_tabstrip_ = NULL;
}

gfx::Point DraggedTabControllerGtk::ConvertScreenPointToTabStripPoint(
    TabStripGtk* tabstrip, const gfx::Point& screen_point) {
  gfx::Point tabstrip_screen_point =
      gtk_util::GetWidgetScreenPosition(tabstrip->tabstrip_.get());
  return screen_point.Subtract(tabstrip_screen_point);
}

gfx::Rect DraggedTabControllerGtk::GetDraggedViewTabStripBounds(
    const gfx::Point& screen_point) {
  gfx::Point client_point =
      ConvertScreenPointToTabStripPoint(attached_tabstrip_, screen_point);
  gfx::Size tab_size = dragged_view_->attached_tab_size();
  return gfx::Rect(client_point.x(), client_point.y(),
                   dragged_view_->GetTotalWidthInTabStrip(), tab_size.height());
}

int DraggedTabControllerGtk::GetInsertionIndexForDraggedBounds(
    const gfx::Rect& dragged_bounds) {
  int dragged_bounds_start = gtk_util::MirroredLeftPointForRect(
      attached_tabstrip_->widget(),
      dragged_bounds);
  int dragged_bounds_end = gtk_util::MirroredRightPointForRect(
      attached_tabstrip_->widget(),
      dragged_bounds);
  int right_tab_x = 0;
  int index = -1;

  for (int i = 0; i < attached_tabstrip_->GetTabCount(); i++) {
    // Divides each tab into two halves to see if the dragged tab has crossed
    // the halfway boundary necessary to move past the next tab.
    gfx::Rect ideal_bounds = attached_tabstrip_->GetIdealBounds(i);
    gfx::Rect left_half, right_half;
    ideal_bounds.SplitVertically(&left_half, &right_half);
    right_tab_x = right_half.x();

    if (dragged_bounds_start >= right_half.x() &&
        dragged_bounds_start < right_half.right()) {
      index = i + 1;
      break;
    } else if (dragged_bounds_start >= left_half.x() &&
               dragged_bounds_start < left_half.right()) {
      index = i;
      break;
    }
  }

  if (index == -1) {
    if (dragged_bounds_end > right_tab_x)
      index = attached_tabstrip_->GetTabCount();
    else
      index = 0;
  }

  TabGtk* tab = GetTabMatchingDraggedContents(
      attached_tabstrip_, drag_data_->GetSourceTabContentsWrapper());
  if (tab == NULL) {
    // If dragged tabs are not attached yet, we don't need to constrain the
    // index.
    return index;
  }

  int max_index =
      attached_tabstrip_-> GetTabCount() - static_cast<int>(drag_data_->size());
  return std::max(0, std::min(max_index, index));
}

gfx::Point DraggedTabControllerGtk::GetDraggedViewPoint(
    const gfx::Point& screen_point) {
  int x = screen_point.x() -
      dragged_view_->GetWidthInTabStripUpToMousePointer();
  int y = screen_point.y() - mouse_offset_.y();

  // If we're not attached, we just use x and y from above.
  if (attached_tabstrip_) {
    gfx::Rect tabstrip_bounds =
        gtk_util::GetWidgetScreenBounds(attached_tabstrip_->tabstrip_.get());
    // Snap the dragged tab to the tabstrip if we are attached, detaching
    // only when the mouse position (screen_point) exceeds the screen bounds
    // of the tabstrip.
    if (x < tabstrip_bounds.x() && screen_point.x() >= tabstrip_bounds.x())
      x = tabstrip_bounds.x();

    gfx::Size tab_size = dragged_view_->attached_tab_size();
    int vertical_drag_magnetism = tab_size.height() * 2;
    int vertical_detach_point = tabstrip_bounds.y() - vertical_drag_magnetism;
    if (y < tabstrip_bounds.y() && screen_point.y() >= vertical_detach_point)
      y = tabstrip_bounds.y();

    // Make sure the tab can't be dragged off the right side of the tabstrip
    // unless the mouse pointer passes outside the bounds of the strip by
    // clamping the position of the dragged window to the tabstrip width less
    // the width of one tab until the mouse pointer (screen_point) exceeds the
    // screen bounds of the tabstrip.
    int max_x =
        tabstrip_bounds.right() - dragged_view_->GetTotalWidthInTabStrip();
    int max_y = tabstrip_bounds.bottom() - tab_size.height();
    if (x > max_x && screen_point.x() <= tabstrip_bounds.right())
      x = max_x;
    if (y > max_y && screen_point.y() <=
        (tabstrip_bounds.bottom() + vertical_drag_magnetism)) {
      y = max_y;
    }
#if defined(OS_CHROMEOS)
    // We don't allow detaching on chromeos. This restricts dragging to the
    // source window.
    x = std::min(std::max(x, tabstrip_bounds.x()), max_x);
    y = tabstrip_bounds.y();
#endif
  }
  return gfx::Point(x, y);
}

int DraggedTabControllerGtk::NormalizeIndexToAttachedTabStrip(int index) const {
  if (index >= attached_tabstrip_->model_->count())
    return attached_tabstrip_->model_->count() - 1;
  if (index == TabStripModel::kNoTab)
    return 0;
  return index;
}

TabGtk* DraggedTabControllerGtk::GetTabMatchingDraggedContents(
    TabStripGtk* tabstrip, TabContentsWrapper* tab_contents) {
  int index = tabstrip->model()->GetIndexOfTabContents(tab_contents);
  return index == TabStripModel::kNoTab ? NULL : tabstrip->GetTabAt(index);
}

std::vector<TabGtk*> DraggedTabControllerGtk::GetTabsMatchingDraggedContents(
    TabStripGtk* tabstrip) {
  std::vector<TabGtk*> dragged_tabs;
  for (size_t i = 0; i < drag_data_->size(); ++i) {
    if (!drag_data_->get(i)->contents_)
      continue;
    TabGtk* tab = GetTabMatchingDraggedContents(tabstrip,
                                                drag_data_->get(i)->contents_);
    if (tab)
      dragged_tabs.push_back(tab);
  }
  return dragged_tabs;
}

void DraggedTabControllerGtk::SetDraggedTabsVisible(bool visible,
                                                    bool repaint) {
  std::vector<TabGtk*> dragged_tabs(GetTabsMatchingDraggedContents(
      attached_tabstrip_));
  for (size_t i = 0; i < dragged_tabs.size(); ++i) {
    dragged_tabs[i]->SetVisible(visible);
    dragged_tabs[i]->set_dragging(!visible);
    if (repaint)
      dragged_tabs[i]->SchedulePaint();
  }
}

bool DraggedTabControllerGtk::EndDragImpl(EndDragType type) {
  bring_to_front_timer_.Stop();

  // WARNING: this may be invoked multiple times. In particular, if deletion
  // occurs after a delay (as it does when the tab is released in the original
  // tab strip) and the navigation controller/tab contents is deleted before
  // the animation finishes, this is invoked twice. The second time through
  // type == TAB_DESTROYED.

  bool destroy_now = true;
  if (type != TAB_DESTROYED) {
    // If we never received a drag-motion event, the drag will never have
    // started in the sense that |dragged_view_| will be NULL. We don't need to
    // revert or complete the drag in that case.
    if (dragged_view_.get()) {
      if (type == CANCELED) {
        RevertDrag();
      } else {
        destroy_now = CompleteDrag();
      }
    }
  } else if (drag_data_->size() > 0) {
    RevertDrag();
  }

  ResetDelegates();

  // If we're not destroyed now, we'll be destroyed asynchronously later.
  if (destroy_now)
    source_tabstrip_->DestroyDragController();

  return destroy_now;
}

void DraggedTabControllerGtk::RevertDrag() {
  // We save this here because code below will modify |attached_tabstrip_|.
  bool restore_window = attached_tabstrip_ != source_tabstrip_;
  if (attached_tabstrip_) {
    if (attached_tabstrip_ != source_tabstrip_) {
      // The tabs were inserted into another tabstrip. We need to put them back
      // into the original one.
      for (size_t i = 0; i < drag_data_->size(); ++i) {
        if (!drag_data_->get(i)->contents_)
          continue;
        int index = attached_tabstrip_->model()->GetIndexOfTabContents(
            drag_data_->get(i)->contents_);
        attached_tabstrip_->model()->DetachTabContentsAt(index);
      }
      // TODO(beng): (Cleanup) seems like we should use Attach() for this
      //             somehow.
      attached_tabstrip_ = source_tabstrip_;
      for (size_t i = 0; i < drag_data_->size(); ++i) {
        if (!drag_data_->get(i)->contents_)
          continue;
        attached_tabstrip_->model()->InsertTabContentsAt(
            drag_data_->get(i)->source_model_index_,
            drag_data_->get(i)->contents_,
            drag_data_->GetAddTypesForDraggedTabAt(i));
      }
    } else {
      // The tabs were moved within the tabstrip where the drag was initiated.
      // Move them back to their starting locations.
      for (size_t i = 0; i < drag_data_->size(); ++i) {
        if (!drag_data_->get(i)->contents_)
          continue;
        int index = attached_tabstrip_->model()->GetIndexOfTabContents(
            drag_data_->get(i)->contents_);
        source_tabstrip_->model()->MoveTabContentsAt(
            index, drag_data_->get(i)->source_model_index_, true);
      }
    }
  } else {
    // TODO(beng): (Cleanup) seems like we should use Attach() for this
    //             somehow.
    attached_tabstrip_ = source_tabstrip_;
    // The tab was detached from the tabstrip where the drag began, and has not
    // been attached to any other tabstrip. We need to put it back into the
    // source tabstrip.
    for (size_t i = 0; i < drag_data_->size(); ++i) {
      if (!drag_data_->get(i)->contents_)
        continue;
      source_tabstrip_->model()->InsertTabContentsAt(
          drag_data_->get(i)->source_model_index_,
          drag_data_->get(i)->contents_,
          drag_data_->GetAddTypesForDraggedTabAt(i));
    }
  }
  RestoreSelection(source_tabstrip_->model());

  // If we're not attached to any tab strip, or attached to some other tab
  // strip, we need to restore the bounds of the original tab strip's frame, in
  // case it has been hidden.
  if (restore_window)
    ShowWindow();

  SetDraggedTabsVisible(true, false);
}

bool DraggedTabControllerGtk::CompleteDrag() {
  bool destroy_immediately = true;
  if (attached_tabstrip_) {
    // We don't need to do anything other than make the tabs visible again,
    // since the dragged view is going away.
    dragged_view_->AnimateToBounds(GetAnimateBounds(),
        base::Bind(&DraggedTabControllerGtk::OnAnimateToBoundsComplete,
                   base::Unretained(this)));
    destroy_immediately = false;
  } else {
    // Compel the model to construct a new window for the detached TabContents.
    BrowserWindowGtk* window = source_tabstrip_->window();
    gfx::Rect window_bounds = window->GetRestoredBounds();
    window_bounds.set_origin(GetWindowCreatePoint());
    Browser* new_browser =
        source_tabstrip_->model()->delegate()->CreateNewStripWithContents(
            drag_data_->get(0)->contents_,
            window_bounds,
            dock_info_,
            window->IsMaximized());
    TabStripModel* new_model = new_browser->tabstrip_model();
    new_model->SetTabPinned(
        new_model->GetIndexOfTabContents(drag_data_->get(0)->contents_),
        drag_data_->get(0)->pinned_);
    for (size_t i = 1; i < drag_data_->size(); ++i) {
      new_model->InsertTabContentsAt(static_cast<int>(i),
                                     drag_data_->get(i)->contents_,
                                     drag_data_->GetAddTypesForDraggedTabAt(i));
    }
    RestoreSelection(new_model);
    new_browser->window()->Show();
    CleanUpHiddenFrame();
  }

  return destroy_immediately;
}

void DraggedTabControllerGtk::ResetDelegates() {
  for (size_t i = 0; i < drag_data_->size(); ++i) {
    if (drag_data_->get(i)->contents_ &&
        drag_data_->get(i)->contents_->tab_contents()->delegate() == this) {
      drag_data_->get(i)->ResetDelegate();
    }
  }
}

void DraggedTabControllerGtk::EnsureDraggedView() {
  if (!dragged_view_.get()) {
    gfx::Rect rect;
    drag_data_->GetSourceTabContents()->GetContainerBounds(&rect);
    dragged_view_.reset(new DraggedViewGtk(drag_data_.get(), mouse_offset_,
                                           rect.size()));
  }
}

gfx::Rect DraggedTabControllerGtk::GetAnimateBounds() {
  // A hidden widget moved with gtk_fixed_move in a GtkFixed container doesn't
  // update its allocation until after the widget is shown, so we have to use
  // the tab bounds we keep track of.
  //
  // We use the requested bounds instead of the allocation because the
  // allocation is relative to the first windowed widget ancestor of the tab.
  // Because of this, we can't use the tabs allocation to get the screen bounds.
  std::vector<TabGtk*> tabs = GetTabsMatchingDraggedContents(
      attached_tabstrip_);
  TabGtk* tab = !base::i18n::IsRTL() ? tabs.front() : tabs.back();
  gfx::Rect bounds = tab->GetRequisition();
  GtkWidget* widget = tab->widget();
  GtkWidget* parent = gtk_widget_get_parent(widget);
  gfx::Point point = gtk_util::GetWidgetScreenPosition(parent);
  bounds.Offset(point);

  return gfx::Rect(bounds.x(), bounds.y(),
                   dragged_view_->GetTotalWidthInTabStrip(), bounds.height());
}

void DraggedTabControllerGtk::HideWindow() {
  GtkWidget* tabstrip = source_tabstrip_->widget();
  GtkWindow* window = platform_util::GetTopLevel(tabstrip);
  gtk_widget_hide(GTK_WIDGET(window));
}

void DraggedTabControllerGtk::ShowWindow() {
  GtkWidget* tabstrip = source_tabstrip_->widget();
  GtkWindow* window = platform_util::GetTopLevel(tabstrip);
  gtk_window_present(window);
}

void DraggedTabControllerGtk::CleanUpHiddenFrame() {
  // If the model we started dragging from is now empty, we must ask the
  // delegate to close the frame.
  if (source_tabstrip_->model()->empty())
    source_tabstrip_->model()->delegate()->CloseFrameAfterDragSession();
}

void DraggedTabControllerGtk::CleanUpDraggedTabs() {
  // If we were attached to the source tabstrip, dragged tabs will be in use. If
  // we were detached or attached to another tabstrip, we can safely remove
  // them and delete them now.
  if (attached_tabstrip_ != source_tabstrip_) {
    for (size_t i = 0; i < drag_data_->size(); ++i) {
      if (drag_data_->get(i)->contents_) {
        registrar_.Remove(
            this, content::NOTIFICATION_TAB_CONTENTS_DESTROYED,
            content::Source<TabContents>(
                drag_data_->get(i)->contents_->tab_contents()));
      }
      source_tabstrip_->DestroyDraggedTab(drag_data_->get(i)->tab_);
      drag_data_->get(i)->tab_ = NULL;
    }
  }
}

void DraggedTabControllerGtk::OnAnimateToBoundsComplete() {
  // Sometimes, for some reason, in automation we can be called back on a
  // detach even though we aren't attached to a tabstrip. Guard against that.
  if (attached_tabstrip_)
    SetDraggedTabsVisible(true, true);

  CleanUpHiddenFrame();

  if (!in_destructor_)
    source_tabstrip_->DestroyDragController();
}

void DraggedTabControllerGtk::BringWindowUnderMouseToFront() {
  // If we're going to dock to another window, bring it to the front.
  gfx::NativeWindow window = dock_info_.window();
  if (!window) {
    gfx::NativeView dragged_tab = dragged_view_->widget();
    dock_windows_.insert(dragged_tab);
    window = DockInfo::GetLocalProcessWindowAtPoint(
        gfx::Screen::GetCursorScreenPoint(), dock_windows_);
    dock_windows_.erase(dragged_tab);
  }

  if (window)
    gtk_window_present(GTK_WINDOW(window));
}

bool DraggedTabControllerGtk::AreTabsConsecutive() {
  for (size_t i = 1; i < drag_data_->size(); ++i) {
    if (drag_data_->get(i - 1)->source_model_index_ + 1 !=
        drag_data_->get(i)->source_model_index_) {
      return false;
    }
  }
  return true;
}
