// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_strip_experimental.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <string>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/adapters.h"
#include "base/macros.h"
#include "base/metrics/histogram_macros.h"
#include "base/metrics/user_metrics.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "cc/paint/paint_flags.h"
#include "chrome/browser/defaults.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/layout_constants.h"
#include "chrome/browser/ui/tabs/tab_data_experimental.h"
#include "chrome/browser/ui/tabs/tab_strip_model_experimental.h"
#include "chrome/browser/ui/view_ids.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/new_tab_button.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"
#include "chrome/browser/ui/views/tabs/tab_experimental.h"
#include "chrome/browser/ui/views/tabs/tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/tab_strip_layout.h"
#include "chrome/browser/ui/views/tabs/tab_strip_observer.h"
#include "chrome/browser/ui/views/touch_uma/touch_uma.h"
#include "chrome/grit/generated_resources.h"
#include "chrome/grit/theme_resources.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/effects/SkBlurMaskFilter.h"
#include "third_party/skia/include/effects/SkLayerDrawLooper.h"
#include "third_party/skia/include/pathops/SkPathOps.h"
#include "ui/accessibility/ax_node_data.h"
#include "ui/base/default_theme_provider.h"
#include "ui/base/dragdrop/drag_drop_types.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/models/list_selection_model.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/compositor/compositing_recorder.h"
#include "ui/compositor/paint_recorder.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/animation/animation_container.h"
#include "ui/gfx/animation/throb_animation.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/geometry/rect_conversions.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/path.h"
#include "ui/gfx/scoped_canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/masked_targeter_delegate.h"
#include "ui/views/mouse_watcher_view_host.h"
#include "ui/views/rect_based_targeting_utils.h"
#include "ui/views/view_model_utils.h"
#include "ui/views/view_targeter.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"
#include "ui/views/window/non_client_view.h"

#if defined(OS_WIN)
#include "ui/display/win/screen_win.h"
#include "ui/gfx/win/hwnd_util.h"
#include "ui/views/win/hwnd_util.h"
#endif

using base::UserMetricsAction;
using ui::DropTargetEvent;

namespace {

const int kTabStripAnimationVSlop = 40;

// Size of the drop indicator.
int drop_indicator_width;
int drop_indicator_height;

// Amount of time we delay before resizing after a close from a touch.
const int kTouchResizeLayoutTimeMS = 2000;

#if defined(OS_MACOSX)
const int kPinnedToNonPinnedOffset = 2;
#else
const int kPinnedToNonPinnedOffset = 3;
#endif

// Returns the width needed for the new tab button (and padding).
int GetNewTabButtonWidth() {
  return GetLayoutSize(NEW_TAB_BUTTON).width() -
         GetLayoutConstant(TABSTRIP_NEW_TAB_BUTTON_OVERLAP);
}

// Animation delegate used for any automatic tab movement.  Hides the tab if it
// is not fully visible within the tabstrip area, to prevent overflow clipping.
class TabAnimationDelegate : public gfx::AnimationDelegate {
 public:
  TabAnimationDelegate(TabStripExperimental* tab_strip, TabExperimental* tab);
  ~TabAnimationDelegate() override;

  void AnimationProgressed(const gfx::Animation* animation) override;

 protected:
  TabStripExperimental* tab_strip() { return tab_strip_; }
  TabExperimental* tab() { return tab_; }

 private:
  TabStripExperimental* const tab_strip_;
  TabExperimental* const tab_;

  DISALLOW_COPY_AND_ASSIGN(TabAnimationDelegate);
};

TabAnimationDelegate::TabAnimationDelegate(TabStripExperimental* tab_strip,
                                           TabExperimental* tab)
    : tab_strip_(tab_strip), tab_(tab) {}

TabAnimationDelegate::~TabAnimationDelegate() {}

void TabAnimationDelegate::AnimationProgressed(
    const gfx::Animation* animation) {
}

// Animation delegate used when a dragged tab is released. When done sets the
// dragging state to false.
class ResetDraggingStateDelegate : public TabAnimationDelegate {
 public:
  ResetDraggingStateDelegate(TabStripExperimental* tab_strip,
                             TabExperimental* tab);
  ~ResetDraggingStateDelegate() override;

  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(ResetDraggingStateDelegate);
};

ResetDraggingStateDelegate::ResetDraggingStateDelegate(
    TabStripExperimental* tab_strip,
    TabExperimental* tab)
    : TabAnimationDelegate(tab_strip, tab) {}

ResetDraggingStateDelegate::~ResetDraggingStateDelegate() {}

void ResetDraggingStateDelegate::AnimationEnded(
    const gfx::Animation* animation) {
  /* TODO(brettw) dragging.
  tab()->set_dragging(false);
  */
  AnimationProgressed(animation);  // Forces tab visibility to update.
}

void ResetDraggingStateDelegate::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// TabStripExperimental::RemoveTabDelegate
//
// AnimationDelegate used when removing a tab. Does the necessary cleanup when
// done.
class TabStripExperimental::RemoveTabDelegate : public TabAnimationDelegate {
 public:
  RemoveTabDelegate(TabStripExperimental* tab_strip, TabExperimental* tab);

  void AnimationEnded(const gfx::Animation* animation) override;
  void AnimationCanceled(const gfx::Animation* animation) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(RemoveTabDelegate);
};

TabStripExperimental::RemoveTabDelegate::RemoveTabDelegate(
    TabStripExperimental* tab_strip,
    TabExperimental* tab)
    : TabAnimationDelegate(tab_strip, tab) {}

void TabStripExperimental::RemoveTabDelegate::AnimationEnded(
    const gfx::Animation* animation) {
  DCHECK(tab()->closing());
  tab_strip()->RemoveAndDeleteTab(tab());

  // Send the Container a message to simulate a mouse moved event at the current
  // mouse position. This tickles the TabExperimental the mouse is currently
  // over to show the "hot" state of the close button.  Note that this is not
  // required (and indeed may crash!) for removes spawned by non-mouse closes
  // and drag-detaches.
  if (!tab_strip()->IsDragSessionActive() &&
      tab_strip()->ShouldHighlightCloseButtonAfterRemove()) {
    // The widget can apparently be null during shutdown.
    views::Widget* widget = tab_strip()->GetWidget();
    if (widget)
      widget->SynthesizeMouseMoveEvent();
  }
}

void TabStripExperimental::RemoveTabDelegate::AnimationCanceled(
    const gfx::Animation* animation) {
  AnimationEnded(animation);
}

///////////////////////////////////////////////////////////////////////////////
// TabStripExperimental, public:

TabStripExperimental::TabStripExperimental(TabStripModel* model)
    : model_(static_cast<TabStripModelExperimental*>(model)),
      current_inactive_width_(Tab::GetStandardSize().width()),
      current_active_width_(Tab::GetStandardSize().width()),
      animation_container_(new gfx::AnimationContainer()),
      bounds_animator_(this) {
  model_->AddExperimentalObserver(this);

  Init();
  SetEventTargeter(std::make_unique<views::ViewTargeter>(this));
}

TabStripExperimental::~TabStripExperimental() {
  model_->RemoveExperimentalObserver(this);

  for (TabStripObserver& observer : observers())
    observer.TabStripDeleted(this);

  // The animations may reference the tabs. Shut down the animation before we
  // delete the tabs.
  StopAnimating(false);

  DestroyDragController();

  // Make sure we unhook ourselves as a message loop observer so that we don't
  // crash in the case where the user closes the window after closing a tab
  // but before moving the mouse.
  RemoveMessageLoopObserver();

  // The children (tabs) may callback to us from their destructor. Delete them
  // so that if they call back we aren't in a weird state.
  RemoveAllChildViews(true);
}

TabStripImpl* TabStripExperimental::AsTabStripImpl() {
  return nullptr;
}

int TabStripExperimental::GetMaxX() const {
  /* TODO(brettw) new tab button.
  return new_tab_button_bounds_.right();
  */
  NOTIMPLEMENTED();
  return 0;
}

void TabStripExperimental::SetBackgroundOffset(const gfx::Point& offset) {
  /* TODO(brettw) backgrounds.
  for (int i = 0; i < tab_count(); ++i)
    tab_at(i)->set_background_offset(offset);
  new_tab_button_->set_background_offset(offset);
  */
}

bool TabStripExperimental::IsRectInWindowCaption(const gfx::Rect& rect) {
  views::View* v = GetEventHandlerForRect(rect);

  // If there is no control at this location, claim the hit was in the title
  // bar to get a move action.
  if (v == this)
    return true;

  // Check to see if the rect intersects the non-button parts of the new tab
  // button. The button has a non-rectangular shape, so if it's not in the
  // visual portions of the button we treat it as a click to the caption.
  /* TODO(brettw) new tab button.
  gfx::RectF rect_in_new_tab_coords_f(rect);
  View::ConvertRectToTarget(this, new_tab_button_, &rect_in_new_tab_coords_f);
  gfx::Rect rect_in_new_tab_coords =
      gfx::ToEnclosingRect(rect_in_new_tab_coords_f);
  if (new_tab_button_->GetLocalBounds().Intersects(rect_in_new_tab_coords) &&
      !new_tab_button_->HitTestRect(rect_in_new_tab_coords))
    return true;
  */

  // All other regions, including the new TabExperimental button, should be
  // considered part of the containing Window's client area so that regular
  // events can be processed for them.
  return false;
}

bool TabStripExperimental::IsPositionInWindowCaption(const gfx::Point& point) {
  return IsRectInWindowCaption(gfx::Rect(point, gfx::Size(1, 1)));
}

bool TabStripExperimental::IsTabStripCloseable() const {
  return !IsDragSessionActive();
}

bool TabStripExperimental::IsTabStripEditable() const {
  return !IsDragSessionActive() && !IsActiveDropTarget();
}

bool TabStripExperimental::IsTabCrashed(int tab_index) const {
  /* TODO(brettw) tab data.
  return tab_at(tab_index)->data().IsCrashed();
  */
  return false;
}

bool TabStripExperimental::TabHasNetworkError(int tab_index) const {
  /* TODO(brettw) tab data.
  return tab_at(tab_index)->data().network_state ==
         TabRendererData::NETWORK_STATE_ERROR;
  */
  return false;
}

TabAlertState TabStripExperimental::GetTabAlertState(int tab_index) const {
  /* TODO(brettw) tab data.
  return tab_at(tab_index)->data().alert_state;
  */
  return TabAlertState::NONE;
}

void TabStripExperimental::UpdateLoadingAnimations() {
  // controller_->UpdateLoadingAnimations();
}

gfx::Rect TabStripExperimental::GetNewTabButtonBounds() {
  /* TODO(brettw) new tab button.
  return new_tab_button_->bounds();
  */
  NOTIMPLEMENTED();
  return gfx::Rect();
}

bool TabStripExperimental::SizeTabButtonToTopOfTabStrip() {
  // Extend the button to the screen edge in maximized and immersive fullscreen.
  views::Widget* widget = GetWidget();
  return browser_defaults::kSizeTabButtonToTopOfTabStrip ||
         (widget && (widget->IsMaximized() || widget->IsFullscreen()));
}

void TabStripExperimental::StartHighlight(int model_index) {
  /* TODO(brettw) pulsing.
  tab_at(model_index)->StartPulse();
  */
}

void TabStripExperimental::StopAllHighlighting() {
  /* TODO(brettw) pulsing.
  for (int i = 0; i < tab_count(); ++i)
    tab_at(i)->StopPulse();
  */
}

/* TODO(brettw) dragging.
void TabStripExperimental::MoveTab(int from_model_index,
                           int to_model_index,
                           const TabRendererData& data) {
  DCHECK_GT(tabs_.view_size(), 0);
  const TabExperimental* last_tab = GetLastVisibleTab();
  tab_at(from_model_index)->SetData(data);
  if (touch_layout_) {
    tabs_.MoveViewOnly(from_model_index, to_model_index);
    int pinned_count = 0;
    GenerateIdealBoundsForPinnedTabs(&pinned_count);
    touch_layout_->MoveTab(from_model_index, to_model_index,
                           GetActiveIndex(),
                           GetStartXForNormalTabs(), pinned_count);
  } else {
    tabs_.Move(from_model_index, to_model_index);
  }
  StartMoveTabAnimation();
  if (TabDragController::IsAttachedTo(this) &&
      (last_tab != GetLastVisibleTab() || last_tab->dragging())) {
    new_tab_button_->SetVisible(false);
  }

  for (TabStripObserver& observer : observers())
    observer.TabStripMovedTab(this, from_model_index, to_model_index);
}
*/

void TabStripExperimental::PrepareForCloseAt(int model_index,
                                             CloseTabSource source) {
  /* TODO(brettw) probably need this in some way.
  if (!in_tab_close_ && IsAnimating()) {
    // Cancel any current animations. We do this as remove uses the current
    // ideal bounds and we need to know ideal bounds is in a good state.
    StopAnimating(true);
  }

  if (!GetWidget())
    return;

  int model_count = GetModelCount();
  if (model_count > 1 && model_index != model_count - 1) {
    // The user is about to close a tab other than the last tab. Set
    // available_width_for_tabs_ so that if we do a layout we don't position a
    // tab past the end of the second to last tab. We do this so that as the
    // user closes tabs with the mouse a tab continues to fall under the mouse.
    TabExperimental* last_tab = tab_at(model_count - 1);
    TabExperimental* tab_being_removed = tab_at(model_index);
    available_width_for_tabs_ = last_tab->bounds().right() -
                                tab_being_removed->width() +
                                TabExperimental::GetOverlap();
    if (model_index == 0 && tab_being_removed->data().pinned &&
        !tab_at(1)->data().pinned) {
      available_width_for_tabs_ -= kPinnedToNonPinnedOffset;
    }
  }

  in_tab_close_ = true;
  resize_layout_timer_.Stop();
  if (source == CLOSE_TAB_FROM_TOUCH) {
    StartResizeLayoutTabsFromTouchTimer();
  } else {
    AddMessageLoopObserver();
  }
  */
}

void TabStripExperimental::SetTabNeedsAttention(int model_index,
                                                bool attention) {
  /* TODO(brettw) attention.
  tab_at(model_index)->SetTabNeedsAttention(attention);
  */
}

int TabStripExperimental::GetModelCount() const {
  return model_->count();
}

bool TabStripExperimental::IsValidModelIndex(int model_index) const {
  return model_index >= 0 && model_index < model_->count();
}

bool TabStripExperimental::IsDragSessionActive() const {
  return drag_controller_.get() != NULL;
}

bool TabStripExperimental::IsActiveDropTarget() const {
  /* TODO(brettw) dragging.
  for (int i = 0; i < tab_count(); ++i) {
    TabExperimental* tab = tab_at(i);
    if (tab->dragging())
      return true;
  }
  */
  return false;
}

SkAlpha TabStripExperimental::GetInactiveAlpha(bool for_new_tab_button) const {
#if defined(OS_CHROMEOS)
  static const SkAlpha kInactiveTabAlphaAsh = 230;
  const SkAlpha base_alpha = kInactiveTabAlphaAsh;
#else
  static const SkAlpha kInactiveTabAlphaGlass = 200;
  static const SkAlpha kInactiveTabAlphaOpaque = 255;
  const SkAlpha base_alpha = GetWidget()->ShouldWindowContentsBeTransparent()
                                 ? kInactiveTabAlphaGlass
                                 : kInactiveTabAlphaOpaque;
#endif  // OS_CHROMEOS
  static const double kMultiSelectionMultiplier = 0.6;
  return (for_new_tab_button || (model_->selection_model().size() <= 1))
             ? base_alpha
             : static_cast<SkAlpha>(kMultiSelectionMultiplier * base_alpha);
}

bool TabStripExperimental::IsAnimating() const {
  return bounds_animator_.IsAnimating();
}

void TabStripExperimental::StopAnimating(bool layout) {
  if (!IsAnimating())
    return;

  bounds_animator_.Cancel();

  if (layout)
    DoLayout();
}

void TabStripExperimental::FileSupported(const GURL& url, bool supported) {
  if (drop_info_.get() && drop_info_->url == url)
    drop_info_->file_supported = supported;
}

void TabStripExperimental::TabInserted(const TabDataExperimental* data,
                                       bool is_active) {
  InvalidateViewOrder();

  TabExperimental* tab = new TabExperimental(data);
  AddChildView(tab);
  tab->SetVisible(true);
  tabs_.emplace(data, tab);

  EnsureViewOrderUpToDate();

  // Don't animate the first tab, it looks weird, and don't animate anything
  // if the containing window isn't visible yet.
  if (view_order_.size() > 1 && GetWidget() && GetWidget()->IsVisible())
    StartInsertTabAnimation(data, tab);
  else
    DoLayout();

  /* TODO(brettw) observer.
  for (TabStripObserver& observer : observers())
    observer.TabStripAddedTabAt(this, model_index);
  */

  // Stop dragging when a new tab is added and dragging a window. Doing
  // otherwise results in a confusing state if the user attempts to reattach. We
  // could allow this and make TabDragController update itself during the add,
  // but this comes up infrequently enough that it's not worth the complexity.
  //
  // At the start of AddTabAt() the model and tabs are out sync. Any queries to
  // find a tab given a model index can go off the end of |tabs_|. As such, it
  // is important that we complete the drag *after* adding the tab so that the
  // model and tabstrip are in sync.
  /* TODO(brettw) dragging.
  if (drag_controller_.get() && !drag_controller_->is_mutating() &&
      drag_controller_->is_dragging_window()) {
    EndDrag(END_DRAG_COMPLETE);
  }
  */
}

void TabStripExperimental::TabClosing(const TabDataExperimental* data) {
  // Don't invalidate the view order yet. The code that starts the animation
  // needs to see the previous layout.

  TabExperimental* tab = TabForData(data);
  if (!tab)
    return;

  if (in_tab_close_ && tab->view_order() != view_order_.size() - 1)
    StartMouseInitiatedRemoveTabAnimation(tab);
  else
    StartRemoveTabAnimation(tab);

  /* TODO(brettw) observer.
  for (TabStripObserver& observer : observers())
    observer.TabStripRemovedTabAt(this, model_index);
  */

  // Stop dragging when a new tab is removed and dragging a window. Doing
  // otherwise results in a confusing state if the user attempts to reattach. We
  // could allow this and make TabDragController update itself during the
  // remove operation, but this comes up infrequently enough that it's not worth
  // the complexity.
  //
  // At the start of RemoveTabAt() the model and tabs are out sync. Any queries
  // to find a tab given a model index can go off the end of |tabs_|. As such,
  // it is important that we complete the drag *after* removing the tab so that
  // the model and tabstrip are in sync.
  /* TODO(brettw) dragging.
  if (contents && drag_controller_.get() && !drag_controller_->is_mutating() &&
      drag_controller_->IsDraggingTab(contents)) {
    EndDrag(END_DRAG_COMPLETE);
  }
  */
}

void TabStripExperimental::TabChanged(const TabDataExperimental* data) {
  TabExperimental* tab = TabForData(data);
  if (tab)
    tab->DataUpdated();
}

void TabStripExperimental::TabSelectionChanged(
    const TabDataExperimental* old_data,
    const TabDataExperimental* new_data) {
  TabExperimental* old_tab = TabForData(old_data);
  if (old_tab) {
    old_tab->SetActive(false);
    old_tab->NotifyAccessibilityEvent(ui::AX_EVENT_SELECTION_REMOVE, true);
  }

  TabExperimental* new_tab = TabForData(new_data);
  if (new_tab) {
    new_tab->SetActive(true);
    new_tab->NotifyAccessibilityEvent(ui::AX_EVENT_SELECTION_ADD, true);
    new_tab->NotifyAccessibilityEvent(ui::AX_EVENT_SELECTION, true);
  }

  // TODO(brettw) some special handling of "tiny tabs" where the selection
  // can change the size of the tab.
  if (!IsAnimating() && !in_tab_close_)
    DoLayout();
  else
    SchedulePaint();
}

void TabStripExperimental::MouseMovedOutOfHost() {
  ResizeLayoutTabs();
}

///////////////////////////////////////////////////////////////////////////////
// TabStripExperimental, views::View overrides:

void TabStripExperimental::Layout() {
  // Only do a layout if our size changed.
  if (last_layout_size_ == size())
    return;
  if (IsDragSessionActive())
    return;
  DoLayout();
}

void TabStripExperimental::PaintChildren(const views::PaintInfo& paint_info) {
  EnsureViewOrderUpToDate();

  // The order stored by the view itself doesn't match the paint order. The
  // paint order is the reverse of the view_order_ vector (to paint
  // back-to-front).
  for (TabExperimental* tab : base::Reversed(view_order_))
    tab->Paint(paint_info);

  /* TODO(brettw) I have no idea what this does.

  // Keep the recording scales consistent for the tab strip and its children.
  // See crbug/753911
  ui::PaintRecorder recorder(paint_info.context(),
                             paint_info.paint_recording_size(),
                             paint_info.paint_recording_scale_x(),
                             paint_info.paint_recording_scale_y(), nullptr);

  gfx::Canvas* canvas = recorder.canvas();
  if (active_tab) {
    canvas->sk_canvas()->clipRect(
        gfx::RectToSkRect(active_tab->GetMirroredBounds()),
        SkClipOp::kDifference);
  }
  */
  /* TODO(brettw) top of toolbar.
  BrowserView::Paint1pxHorizontalLine(canvas, GetToolbarTopSeparatorColor(),
                                      GetLocalBounds(), true);
  */
}

const char* TabStripExperimental::GetClassName() const {
  static const char kViewClassName[] = "TabStrip";
  return kViewClassName;
}

gfx::Size TabStripExperimental::CalculatePreferredSize() const {
  EnsureViewOrderUpToDate();

  int needed_tab_width;

  int tab_count = static_cast<int>(view_order_.size());
  int pinned_tab_count = GetPinnedTabCount();

  needed_tab_width = pinned_tab_count * Tab::GetPinnedWidth();
  const int remaining_tab_count = tab_count - pinned_tab_count;
  const int min_selected_width = Tab::GetMinimumActiveSize().width();
  const int min_unselected_width = Tab::GetMinimumInactiveSize().width();
  if (remaining_tab_count > 0) {
    needed_tab_width += kPinnedToNonPinnedOffset + min_selected_width +
                        ((remaining_tab_count - 1) * min_unselected_width);
  }

  const int overlap = TabExperimental::GetOverlap();
  if (tab_count > 1)
    needed_tab_width -= (tab_count - 1) * overlap;

  // Don't let the tabstrip shrink smaller than is necessary to show one tab,
  // and don't force it to be larger than is necessary to show 20 tabs.
  const int largest_min_tab_width =
      min_selected_width + 19 * (min_unselected_width - overlap);
  needed_tab_width = std::min(std::max(needed_tab_width, min_selected_width),
                              largest_min_tab_width);

  return gfx::Size(needed_tab_width + GetNewTabButtonWidth(),
                   Tab::GetMinimumInactiveSize().height());
}

void TabStripExperimental::OnDragEntered(const DropTargetEvent& event) {
  // Force animations to stop, otherwise it makes the index calculation tricky.
  StopAnimating(true);

  UpdateDropIndex(event);

  GURL url;
  base::string16 title;

  // Check whether the event data includes supported drop data.
  /* TODO(brettw) tab data.
  if (event.data().GetURLAndTitle(ui::OSExchangeData::CONVERT_FILENAMES, &url,
                                  &title) &&
      url.is_valid()) {
    drop_info_->url = url;

    // For file:// URLs, kick off a MIME type request in case they're dropped.
    if (url.SchemeIsFile())
      controller_->CheckFileSupported(url);
  }
  */
}

int TabStripExperimental::OnDragUpdated(const DropTargetEvent& event) {
  // Update the drop index even if the file is unsupported, to allow
  // dragging a file to the contents of another tab.
  UpdateDropIndex(event);

  if (!drop_info_->file_supported ||
      drop_info_->url.SchemeIs(url::kJavaScriptScheme))
    return ui::DragDropTypes::DRAG_NONE;

  return GetDropEffect(event);
}

void TabStripExperimental::OnDragExited() {
  SetDropIndex(-1, false);
}

int TabStripExperimental::OnPerformDrop(const DropTargetEvent& event) {
  /* TODO(brettw) tab data.
  if (!drop_info_.get())
    return ui::DragDropTypes::DRAG_NONE;

  const int drop_index = drop_info_->drop_index;
  const bool drop_before = drop_info_->drop_before;
  const bool file_supported = drop_info_->file_supported;

  // Hide the drop indicator.
  SetDropIndex(-1, false);

  // Do nothing if the file was unsupported, the URL is invalid, or this is a
  // javascript: URL (prevent self-xss). The URL may have been changed after
  // |drop_info_| was created.
  GURL url;
  base::string16 title;
  if (!file_supported ||
      !event.data().GetURLAndTitle(ui::OSExchangeData::CONVERT_FILENAMES, &url,
                                   &title) ||
      !url.is_valid() || url.SchemeIs(url::kJavaScriptScheme))
    return ui::DragDropTypes::DRAG_NONE;

  // controller_->PerformDrop(drop_before, drop_index, url);

  return GetDropEffect(event);
  */
  return ui::DragDropTypes::DRAG_NONE;
}

void TabStripExperimental::GetAccessibleNodeData(ui::AXNodeData* node_data) {
  node_data->role = ui::AX_ROLE_TAB_LIST;
}

views::View* TabStripExperimental::GetTooltipHandlerForPoint(
    const gfx::Point& point) {
  if (!HitTestPoint(point))
    return NULL;

  // Return any view that isn't a TabExperimental or this TabStrip
  // immediately. We don't want to interfere.
  views::View* v = View::GetTooltipHandlerForPoint(point);
  if (v && v != this &&
      strcmp(v->GetClassName(), TabExperimental::kViewClassName))
    return v;

  views::View* tab = FindTabHitByPoint(point);
  if (tab)
    return tab;
  return this;
}

///////////////////////////////////////////////////////////////////////////////
// TabStripExperimental, private:

int TabStripExperimental::GetActiveIndex() const {
  return model_->selection_model().active();
}

void TabStripExperimental::Init() {
  set_id(VIEW_ID_TAB_STRIP);
  // So we get enter/exit on children to switch stacked layout on and off.
  set_notify_enter_exit_on_child(true);

  /* TODO(brettw) new tab button.
  new_tab_button_bounds_.set_size(GetLayoutSize(NEW_TAB_BUTTON));
  new_tab_button_bounds_.Inset(0, 0, 0, -NewTabButton::GetTopOffset());
  new_tab_button_ = new NewTabButton(this, this);
  new_tab_button_->SetTooltipText(
      l10n_util::GetStringUTF16(IDS_TOOLTIP_NEW_TAB));
  new_tab_button_->SetAccessibleName(
      l10n_util::GetStringUTF16(IDS_ACCNAME_NEWTAB));
  new_tab_button_->SetImageAlignment(views::ImageButton::ALIGN_LEFT,
                                     views::ImageButton::ALIGN_BOTTOM);
  new_tab_button_->SetEventTargeter(
      std::make_unique<views::ViewTargeter>(new_tab_button_));
  AddChildView(new_tab_button_);
  */

  if (drop_indicator_width == 0) {
    // Direction doesn't matter, both images are the same size.
    gfx::ImageSkia* drop_image = GetDropArrowImage(true);
    drop_indicator_width = drop_image->width();
    drop_indicator_height = drop_image->height();
  }
}

void TabStripExperimental::StartInsertTabAnimation(
    const TabDataExperimental* data,
    TabExperimental* tab) {
  // The TabStrip can now use its entire width to lay out Tabs.
  in_tab_close_ = false;
  available_width_for_tabs_ = -1;

  GenerateIdealBounds();

  // Set the current bounds to be the correct place but 0 width.
  DCHECK(view_order_[tab->view_order()] == tab);
  if (tab->view_order() == 0) {
    tab->SetBounds(0, tab->ideal_bounds().y(), 0, tab->ideal_bounds().height());
  } else {
    TabExperimental* prev_tab = view_order_[tab->view_order() - 1];
    tab->SetBounds(prev_tab->bounds().right() - TabExperimental::GetOverlap(),
                   tab->ideal_bounds().y(), 0, tab->ideal_bounds().height());
  }

  // Animate in to the full width.
  AnimateToIdealBounds();
}

void TabStripExperimental::StartMoveTabAnimation() {
  PrepareForAnimation();
  GenerateIdealBounds();
  AnimateToIdealBounds();
}

void TabStripExperimental::StartRemoveTabAnimation(TabExperimental* tab) {
  PrepareForAnimation();
  tab->SetClosing();

  RemoveTabFromViewModel(tab);
  ScheduleRemoveTabAnimation(tab);
}

void TabStripExperimental::ScheduleRemoveTabAnimation(TabExperimental* tab) {
  // Start an animation for the tabs.
  GenerateIdealBounds();
  AnimateToIdealBounds();

  // Animate the tab being closed to zero width.
  gfx::Rect tab_bounds = tab->bounds();
  tab_bounds.set_width(0);
  bounds_animator_.AnimateViewTo(tab, tab_bounds);
  bounds_animator_.SetAnimationDelegate(
      tab, std::make_unique<RemoveTabDelegate>(this, tab));

  // Don't animate the new tab button when dragging tabs. Otherwise it looks
  // like the new tab button magically appears from beyond the end of the tab
  // strip.
  /* TODO(brettw) new tab button.
  if (TabDragController::IsAttachedTo(this)) {
    bounds_animator_.StopAnimatingView(new_tab_button_);
    new_tab_button_->SetBoundsRect(new_tab_button_bounds_);
  }
  */
}

void TabStripExperimental::AnimateToIdealBounds() {
  EnsureViewOrderUpToDate();
  for (TabExperimental* tab : view_order_) {
    bounds_animator_.AnimateViewTo(tab, tab->ideal_bounds());
    bounds_animator_.SetAnimationDelegate(
        tab, std::make_unique<TabAnimationDelegate>(this, tab));
  }

  /* TODO(brettw) new tab button.
  bounds_animator_.AnimateViewTo(new_tab_button_, new_tab_button_bounds_);
  */
}

bool TabStripExperimental::ShouldHighlightCloseButtonAfterRemove() {
  return in_tab_close_;
}

void TabStripExperimental::DoLayout() {
  last_layout_size_ = size();

  StopAnimating(false);

  GenerateIdealBounds();
  for (TabExperimental* tab : view_order_)
    tab->SetBoundsRect(tab->ideal_bounds());

  SchedulePaint();

  /* TODO(brettw) new tab button.
  bounds_animator_.StopAnimatingView(new_tab_button_);
  new_tab_button_->SetBoundsRect(new_tab_button_bounds_);
  */
}

void TabStripExperimental::DragActiveTab(
    const std::vector<int>& initial_positions,
    int delta) {
  StackDraggedTabs(delta);
}

void TabStripExperimental::StackDraggedTabs(int delta) {
// TODO(brettw) tab dragging
#if 0
  GenerateIdealBounds();
  const int active_index = GetActiveIndex();
  DCHECK_NE(-1, active_index);
  if (delta < 0) {
    // Drag the tabs to the left, stacking tabs before the active tab.
    const int adjusted_delta =
        std::min(ideal_bounds(active_index).x() -
                     kStackedPadding * std::min(active_index, kMaxStackedCount),
                 -delta);
    for (int i = 0; i <= active_index; ++i) {
      const int min_x = std::min(i, kMaxStackedCount) * kStackedPadding;
      gfx::Rect new_bounds(ideal_bounds(i));
      new_bounds.set_x(std::max(min_x, new_bounds.x() - adjusted_delta));
      tabs_.set_ideal_bounds(i, new_bounds);
    }
    /* TODO(brettw) pinned.
    const bool is_active_pinned = tab_at(active_index)->data().pinned;
    */
    const int active_width = ideal_bounds(active_index).width();
    for (int i = active_index + 1; i < tab_count(); ++i) {
      const int max_x =
          ideal_bounds(active_index).x() +
          (kStackedPadding * std::min(i - active_index, kMaxStackedCount));
      gfx::Rect new_bounds(ideal_bounds(i));
      int new_x = std::max(new_bounds.x() + delta, max_x);
      /* TODO(brettw) pinned.
      if (new_x == max_x && !tab_at(i)->data().pinned && !is_active_pinned &&
          new_bounds.width() != active_width)
      */
      if (new_x == max_x && new_bounds.width() != active_width)
        new_x += (active_width - new_bounds.width());
      new_bounds.set_x(new_x);
      tabs_.set_ideal_bounds(i, new_bounds);
    }
  } else {
    // Drag the tabs to the right, stacking tabs after the active tab.
    const int last_tab_width = ideal_bounds(tab_count() - 1).width();
    const int last_tab_x = GetTabAreaWidth() - last_tab_width;
    if (active_index == tab_count() - 1 &&
        ideal_bounds(tab_count() - 1).x() == last_tab_x)
      return;
    const int adjusted_delta =
        std::min(last_tab_x -
                     kStackedPadding * std::min(tab_count() - active_index - 1,
                                                kMaxStackedCount) -
                     ideal_bounds(active_index).x(),
                 delta);
    for (int last_index = tab_count() - 1, i = last_index; i >= active_index;
         --i) {
      const int max_x =
          last_tab_x -
          std::min(tab_count() - i - 1, kMaxStackedCount) * kStackedPadding;
      gfx::Rect new_bounds(ideal_bounds(i));
      int new_x = std::min(max_x, new_bounds.x() + adjusted_delta);
      // Because of rounding not all tabs are the same width. Adjust the
      // position to accommodate this, otherwise the stacking is off.
      /* TODO(brettw) pinned.
      if (new_x == max_x && !tab_at(i)->data().pinned &&
          new_bounds.width() != last_tab_width)
      */
      if (new_x == max_x && new_bounds.width() != last_tab_width)
        new_x += (last_tab_width - new_bounds.width());
      new_bounds.set_x(new_x);
      tabs_.set_ideal_bounds(i, new_bounds);
    }
    for (int i = active_index - 1; i >= 0; --i) {
      const int min_x =
          ideal_bounds(active_index).x() -
          std::min(active_index - i, kMaxStackedCount) * kStackedPadding;
      gfx::Rect new_bounds(ideal_bounds(i));
      new_bounds.set_x(std::min(min_x, new_bounds.x() + delta));
      tabs_.set_ideal_bounds(i, new_bounds);
    }
    /* TODO(brettw) new tab button.
    if (ideal_bounds(tab_count() - 1).right() >= new_tab_button_->x())
      new_tab_button_->SetVisible(false);
    */
  }
  views::ViewModelUtils::SetViewBoundsToIdealBounds(tabs_);
  SchedulePaint();
#endif
}

bool TabStripExperimental::IsStackingDraggedTabs() const {
  return drag_controller_.get() && drag_controller_->started_drag() &&
         (drag_controller_->move_behavior() ==
          TabDragController::MOVE_VISIBLE_TABS);
}

int TabStripExperimental::GetPinnedTabCount() const {
  /* TODO(brettw) pinned.
  int pinned_count = 0;
  while (pinned_count < tab_count() && tab_at(pinned_count)->data().pinned)
    pinned_count++;
  return pinned_count;
  */
  return 0;
}

void TabStripExperimental::RemoveTabFromViewModel(TabExperimental* tab) {
  // We still need to paint the tab until we actually remove it. Put it
  // in tabs_closing_map_ so we can find it.
  closing_tabs_.insert(tab);
  auto found = tabs_.find(tab->data());
  if (found == tabs_.end())
    NOTREACHED();
  else
    tabs_.erase(found);
  InvalidateViewOrder();
}

void TabStripExperimental::RemoveAndDeleteTab(TabExperimental* tab) {
  std::unique_ptr<TabExperimental> deleter(tab);

  auto found = closing_tabs_.find(tab);
  DCHECK(found != closing_tabs_.end());
  closing_tabs_.erase(found);

  InvalidateViewOrder();
}

void TabStripExperimental::DestroyDragController() {
  /* TODO(brettw) new tab button.
  new_tab_button_->SetVisible(true);
  */
  drag_controller_.reset();
}

TabDragController* TabStripExperimental::ReleaseDragController() {
  return drag_controller_.release();
}

void TabStripExperimental::ResizeLayoutTabs() {
  // We've been called back after the TabStrip has been emptied out (probably
  // just prior to the window being destroyed). We need to do nothing here or
  // else GetTabAt below will crash.
  if (tabs_.empty())
    return;

  // It is critically important that this is unhooked here, otherwise we will
  // keep spying on messages forever.
  RemoveMessageLoopObserver();

  in_tab_close_ = false;
  available_width_for_tabs_ = -1;
  /* TODO(brettw) pinned tabs
  int pinned_tab_count = GetPinnedTabCount();
  if (pinned_tab_count == tab_count()) {
    // Only pinned tabs, we know the tab widths won't have changed (all
    // pinned tabs have the same width), so there is nothing to do.
    return;
  }
  */
  // Don't try and avoid layout based on tab sizes. If tabs are small enough
  // then the width of the active tab may not change, but other widths may
  // have. This is particularly important if we've overflowed (all tabs are at
  // the min).
  StartResizeLayoutAnimation();
}

void TabStripExperimental::ResizeLayoutTabsFromTouch() {
  // Don't resize if the user is interacting with the tabstrip.
  if (!drag_controller_.get())
    ResizeLayoutTabs();
  else
    StartResizeLayoutTabsFromTouchTimer();
}

void TabStripExperimental::StartResizeLayoutTabsFromTouchTimer() {
  resize_layout_timer_.Stop();
  resize_layout_timer_.Start(
      FROM_HERE, base::TimeDelta::FromMilliseconds(kTouchResizeLayoutTimeMS),
      this, &TabStripExperimental::ResizeLayoutTabsFromTouch);
}

void TabStripExperimental::SetTabBoundsForDrag(
    const std::vector<gfx::Rect>& tab_bounds) {
  /* TODO(brettw) dragging
  StopAnimating(false);
  DCHECK_EQ(tab_count(), static_cast<int>(tab_bounds.size()));
  for (int i = 0; i < tab_count(); ++i)
    tab_at(i)->SetBoundsRect(tab_bounds[i]);
  */
  // Reset the layout size as we've effectively layed out a different size.
  // This ensures a layout happens after the drag is done.
  last_layout_size_ = gfx::Size();
}

void TabStripExperimental::AddMessageLoopObserver() {
  if (!mouse_watcher_.get()) {
    mouse_watcher_.reset(new views::MouseWatcher(
        new views::MouseWatcherViewHost(
            this, gfx::Insets(0, 0, kTabStripAnimationVSlop, 0)),
        this));
  }
  mouse_watcher_->Start();
}

void TabStripExperimental::RemoveMessageLoopObserver() {
  mouse_watcher_.reset(NULL);
}

gfx::Rect TabStripExperimental::GetDropBounds(int drop_index,
                                              bool drop_before,
                                              bool* is_beneath) {
  /* TODO(brettw) drag and drop.
  DCHECK_NE(drop_index, -1);

  const int overlap = TabExperimental::GetOverlap();
  int center_x;
  if (drop_index < tab_count()) {
    TabExperimental* tab = tab_at(drop_index);
    center_x = tab->x() + ((drop_before ? overlap : tab->width()) / 2);
  } else {
    TabExperimental* last_tab = tab_at(drop_index - 1);
    center_x = last_tab->x() + last_tab->width() - (overlap / 2);
  }

  // Mirror the center point if necessary.
  center_x = GetMirroredXInView(center_x);

  // Determine the screen bounds.
  gfx::Point drop_loc(center_x - drop_indicator_width / 2,
                      -drop_indicator_height);
  ConvertPointToScreen(this, &drop_loc);
  gfx::Rect drop_bounds(drop_loc.x(), drop_loc.y(), drop_indicator_width,
                        drop_indicator_height);

  // If the rect doesn't fit on the monitor, push the arrow to the bottom.
  display::Screen* screen = display::Screen::GetScreen();
  display::Display display = screen->GetDisplayMatching(drop_bounds);
  *is_beneath = !display.bounds().Contains(drop_bounds);
  if (*is_beneath)
    drop_bounds.Offset(0, drop_bounds.height() + height());
  return drop_bounds;
  */
  return gfx::Rect();
}

void TabStripExperimental::UpdateDropIndex(const DropTargetEvent& event) {
  /* TODO(brettw) dragging.
  // If the UI layout is right-to-left, we need to mirror the mouse
  // coordinates since we calculate the drop index based on the
  // original (and therefore non-mirrored) positions of the tabs.
  const int x = GetMirroredXInView(event.x());
  // We don't allow replacing the urls of pinned tabs.
  for (int i = GetPinnedTabCount(); i < tab_count(); ++i) {
    TabExperimental* tab = tab_at(i);
    const int tab_max_x = tab->x() + tab->width();
    const int hot_width = tab->width() / kTabEdgeRatioInverse;
    if (x < tab_max_x) {
      if (x < tab->x() + hot_width)
        SetDropIndex(i, true);
      else if (x >= tab_max_x - hot_width)
        SetDropIndex(i + 1, true);
      else
        SetDropIndex(i, false);
      return;
    }
  }

  // The drop isn't over a tab, add it to the end.
  SetDropIndex(tab_count(), true);
  */
}

void TabStripExperimental::SetDropIndex(int tab_data_index, bool drop_before) {
  // Let the controller know of the index update.
  // controller_->OnDropIndexUpdate(tab_data_index, drop_before);

  if (tab_data_index == -1) {
    if (drop_info_.get())
      drop_info_.reset(NULL);
    return;
  }

  if (drop_info_.get() && drop_info_->drop_index == tab_data_index &&
      drop_info_->drop_before == drop_before) {
    return;
  }

  bool is_beneath;
  gfx::Rect drop_bounds =
      GetDropBounds(tab_data_index, drop_before, &is_beneath);

  if (!drop_info_.get()) {
    drop_info_.reset(
        new DropInfo(tab_data_index, drop_before, !is_beneath, GetWidget()));
  } else {
    drop_info_->drop_index = tab_data_index;
    drop_info_->drop_before = drop_before;
    if (is_beneath == drop_info_->point_down) {
      drop_info_->point_down = !is_beneath;
      drop_info_->arrow_view->SetImage(
          GetDropArrowImage(drop_info_->point_down));
    }
  }

  // Reposition the window. Need to show it too as the window is initially
  // hidden.
  drop_info_->arrow_window->SetBounds(drop_bounds);
  drop_info_->arrow_window->Show();
}

int TabStripExperimental::GetDropEffect(const ui::DropTargetEvent& event) {
  const int source_ops = event.source_operations();
  if (source_ops & ui::DragDropTypes::DRAG_COPY)
    return ui::DragDropTypes::DRAG_COPY;
  if (source_ops & ui::DragDropTypes::DRAG_LINK)
    return ui::DragDropTypes::DRAG_LINK;
  return ui::DragDropTypes::DRAG_MOVE;
}

// static
gfx::ImageSkia* TabStripExperimental::GetDropArrowImage(bool is_down) {
  return ui::ResourceBundle::GetSharedInstance().GetImageSkiaNamed(
      is_down ? IDR_TAB_DROP_DOWN : IDR_TAB_DROP_UP);
}

// TabStripExperimental::DropInfo
// ----------------------------------------------------------

TabStripExperimental::DropInfo::DropInfo(int drop_index,
                                         bool drop_before,
                                         bool point_down,
                                         views::Widget* context)
    : drop_index(drop_index),
      drop_before(drop_before),
      point_down(point_down),
      file_supported(true) {
  arrow_view = new views::ImageView;
  arrow_view->SetImage(GetDropArrowImage(point_down));

  arrow_window = new views::Widget;
  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.keep_on_top = true;
  params.opacity = views::Widget::InitParams::TRANSLUCENT_WINDOW;
  params.accept_events = false;
  params.bounds = gfx::Rect(drop_indicator_width, drop_indicator_height);
  params.context = context->GetNativeWindow();
  arrow_window->Init(params);
  arrow_window->SetContentsView(arrow_view);
}

TabStripExperimental::DropInfo::~DropInfo() {
  // Close eventually deletes the window, which deletes arrow_view too.
  arrow_window->Close();
}

///////////////////////////////////////////////////////////////////////////////

void TabStripExperimental::PrepareForAnimation() {
  /* TODO(brettw) dragging.
  if (!IsDragSessionActive() && !TabDragController::IsAttachedTo(this)) {
    for (int i = 0; i < tab_count(); ++i)
      tab_at(i)->set_dragging(false);
  }
  */
}

void TabStripExperimental::GenerateIdealBounds() {
  if (tabs_.empty())
    return;  // Ignore calls during construction and destruction.

  EnsureViewOrderUpToDate();
  int available_width = (available_width_for_tabs_ < 0)
                            ? GetTabAreaWidth()
                            : available_width_for_tabs_;

  gfx::Size standard_size = Tab::GetStandardSize();
  int tab_width = available_width / view_order_.size();
  if (tab_width > standard_size.width())
    tab_width = standard_size.width();

  int x = 0;
  for (size_t i = 0; i < view_order_.size(); i++) {
    view_order_[i]->set_ideal_bounds(
        gfx::Rect(x, 0, tab_width, standard_size.height()));
    x += tab_width;
  }

  /* TODO(brettw) need to notify of max X.
  for (TabStripObserver& observer : observers())
    observer.TabStripMaxXChanged(this);
  */
}

int TabStripExperimental::GenerateIdealBoundsForPinnedTabs(
    int* first_non_pinned_index) {
  /* TODO(brettw) pinned tabs.
  const int num_pinned_tabs = GetPinnedTabCount();

  if (first_non_pinned_index)
    *first_non_pinned_index = num_pinned_tabs;

  if (num_pinned_tabs == 0)
    return 0;

  std::vector<gfx::Rect> tab_bounds(tab_count());
  CalculateBoundsForPinnedTabs(GetTabSizeInfo(), num_pinned_tabs, tab_count(),
                               &tab_bounds);
  for (int i = 0; i < num_pinned_tabs; ++i)
    tabs_.set_ideal_bounds(i, tab_bounds[i]);
  return (num_pinned_tabs < tab_count())
             ? tab_bounds[num_pinned_tabs].x()
             : tab_bounds[num_pinned_tabs - 1].right() -
                   TabExperimental::GetOverlap();
  */
  return 0;
}

int TabStripExperimental::GetTabAreaWidth() const {
  return width() - GetNewTabButtonWidth();
}

void TabStripExperimental::StartResizeLayoutAnimation() {
  PrepareForAnimation();
  GenerateIdealBounds();
  AnimateToIdealBounds();
}

void TabStripExperimental::StartPinnedTabAnimation() {
  in_tab_close_ = false;
  available_width_for_tabs_ = -1;

  PrepareForAnimation();

  GenerateIdealBounds();
  AnimateToIdealBounds();
}

void TabStripExperimental::StartMouseInitiatedRemoveTabAnimation(
    TabExperimental* tab) {
  // The user initiated the close. We want to persist the bounds of all the
  // existing tabs, so we manually shift ideal_bounds then animate.
  int delta = tab->width() - TabExperimental::GetOverlap();

  /* TODO(brettw) pinned.
  // If the tab being closed is a pinned tab next to a non-pinned tab, be sure
  // to add the extra padding.
  if (tab->data().pinned && !tab_at(model_index + 1)->data().pinned)
    delta += kPinnedToNonPinnedOffset;
  */

  // Set the ideal bounds of everything to be moved over to cover for the
  // removed tab. This does not recompute the ideal bounds so every slides
  // over without expansion until the user moves the mouse away.
  for (size_t i = tab->view_order() + 1; i < view_order_.size(); i++) {
    gfx::Rect bounds = tab->ideal_bounds();
    bounds.set_x(bounds.x() - delta);
    tab->set_ideal_bounds(bounds);
  }

  // Don't just subtract |delta| from the New TabExperimental x-coordinate, as
  // we might have overflow tabs that will be able to animate into the strip, in
  // which case the new tab button should stay where it is.
  /* TODO(brettw) new tab button
  new_tab_button_bounds_.set_x(
      std::min(width() - new_tab_button_bounds_.width(),
               ideal_bounds(tab_count() - 1).right() -
                   GetLayoutConstant(TABSTRIP_NEW_TAB_BUTTON_OVERLAP)));
  */

  PrepareForAnimation();

  tab->SetClosing();

  // We still need to paint the tab until we actually remove it. Put it in
  // tabs_closing_map_ so we can find it.
  RemoveTabFromViewModel(tab);

  AnimateToIdealBounds();

  gfx::Rect tab_bounds = tab->bounds();
  tab_bounds.set_width(0);
  bounds_animator_.AnimateViewTo(tab, tab_bounds);

  // Register delegate to do cleanup when done.
  bounds_animator_.SetAnimationDelegate(
      tab, std::make_unique<RemoveTabDelegate>(this, tab));
}

bool TabStripExperimental::IsPointInTab(
    TabExperimental* tab,
    const gfx::Point& point_in_tabstrip_coords) {
  gfx::Point point_in_tab_coords(point_in_tabstrip_coords);
  View::ConvertPointToTarget(this, tab, &point_in_tab_coords);
  return tab->HitTestPoint(point_in_tab_coords);
}

int TabStripExperimental::GetStartXForNormalTabs() const {
  int pinned_tab_count = GetPinnedTabCount();
  if (pinned_tab_count == 0)
    return 0;
  return pinned_tab_count *
             (Tab::GetPinnedWidth() - TabExperimental::GetOverlap()) +
         kPinnedToNonPinnedOffset;
}

TabExperimental* TabStripExperimental::FindTabHitByPoint(
    const gfx::Point& point) {
  /* TODO(brettw) hit testing.
  // The display order doesn't necessarily match the child order, so we iterate
  // in display order.
  for (int i = 0; i < tab_count(); ++i) {
    // If we don't first exclude points outside the current tab, the code below
    // will return the wrong tab if the next tab is selected, the following tab
    // is active, and |point| is in the overlap region between the two.
    TabExperimental* tab = tab_at(i);
    if (!IsPointInTab(tab, point))
      continue;

    // Selected tabs render atop unselected ones, and active tabs render atop
    // everything.  Check whether the next tab renders atop this one and |point|
    // is in the overlap region.
    TabExperimental* next_tab = i < (tab_count() - 1) ? tab_at(i + 1) : nullptr;
    if (next_tab &&
        (next_tab->active() || (next_tab->selected() && !tab->selected())) &&
        IsPointInTab(next_tab, point))
      return next_tab;

    // This is the topmost tab for this point.
    return tab;
  }
  */
  return nullptr;
}

std::vector<int> TabStripExperimental::GetTabXCoordinates() {
  /*
    std::vector<int> results;
    for (int i = 0; i < tab_count(); ++i)
      results.push_back(ideal_bounds(i).x());
    return results;
    */
  return std::vector<int>();
}

void TabStripExperimental::ButtonPressed(views::Button* sender,
                                         const ui::Event& event) {
  /* TODO(brettw) new tab button.
  if (sender == new_tab_button_) {
    base::RecordAction(UserMetricsAction("NewTab_Button"));
    UMA_HISTOGRAM_ENUMERATION("Tab.NewTab",
  TabStripModel::NEW_TAB_BUTTON, TabStripModel::NEW_TAB_ENUM_COUNT); if
  (event.IsMouseEvent()) { const ui::MouseEvent& mouse = static_cast<const
  ui::MouseEvent&>(event); if (mouse.IsOnlyMiddleMouseButton()) { if
  (ui::Clipboard::IsSupportedClipboardType( ui::CLIPBOARD_TYPE_SELECTION)) {
          ui::Clipboard* clipboard = ui::Clipboard::GetForCurrentThread();
          CHECK(clipboard);
          base::string16 clipboard_text;
          clipboard->ReadText(ui::CLIPBOARD_TYPE_SELECTION, &clipboard_text);
          //if (!clipboard_text.empty())
          //  controller_->CreateNewTabWithLocation(clipboard_text);
        }
        return;
      }
    }

    controller_->CreateNewTab();
    if (event.type() == ui::ET_GESTURE_TAP)
      TouchUMA::RecordGestureAction(TouchUMA::GESTURE_NEWTAB_TAP);
  }
  */
}

// Overridden to support automation. See automation_proxy_uitest.cc.
const views::View* TabStripExperimental::GetViewByID(int view_id) const {
  /* TODO(brettw) automation?
  if (tab_count() > 0) {
    if (view_id == VIEW_ID_TAB_LAST)
      return tab_at(tab_count() - 1);
    if ((view_id >= VIEW_ID_TAB_0) && (view_id < VIEW_ID_TAB_LAST)) {
      int index = view_id - VIEW_ID_TAB_0;
      return (index >= 0 && index < tab_count()) ? tab_at(index) : NULL;
    }
  }

  return View::GetViewByID(view_id);
  */
  return nullptr;
}

bool TabStripExperimental::OnMousePressed(const ui::MouseEvent& event) {
  // We can't return true here, else clicking in an empty area won't drag the
  // window.
  return false;
}

bool TabStripExperimental::OnMouseDragged(const ui::MouseEvent& event) {
  /* TODO(brett) dragging.
  ContinueDrag(this, event);
  */
  return true;
}

void TabStripExperimental::OnMouseReleased(const ui::MouseEvent& event) {
  /* TODO(brettw) dragging.
  EndDrag(END_DRAG_COMPLETE);
  */
}

void TabStripExperimental::OnMouseCaptureLost() {
  /* TODO(brettw) dragging.
  EndDrag(END_DRAG_CAPTURE_LOST);
  */
}

void TabStripExperimental::OnMouseMoved(const ui::MouseEvent& event) {
}

void TabStripExperimental::OnMouseEntered(const ui::MouseEvent& event) {
}

void TabStripExperimental::OnGestureEvent(ui::GestureEvent* event) {
  switch (event->type()) {
    case ui::ET_GESTURE_SCROLL_END:
    case ui::ET_SCROLL_FLING_START:
    case ui::ET_GESTURE_END:
      /* TODO(brettw) dragging.
      EndDrag(END_DRAG_COMPLETE);
      */
      break;

    case ui::ET_GESTURE_LONG_PRESS:
      if (drag_controller_.get())
        drag_controller_->SetMoveBehavior(TabDragController::REORDER);
      break;

    case ui::ET_GESTURE_LONG_TAP: {
      /* TODO(brettw) dragging.
      EndDrag(END_DRAG_CANCEL);
      */

      /* TODO(brettw) context menu.
      gfx::Point local_point = event->location();
      TabExperimental* tab = touch_layout_ ? FindTabForEvent(local_point)
                               : FindTabHitByPoint(local_point);
      if (tab) {
        ConvertPointToScreen(this, &local_point);
        ShowContextMenuForTab(tab, local_point, ui::MENU_SOURCE_TOUCH);
      }
      */
      break;
    }

    case ui::ET_GESTURE_SCROLL_UPDATE:
      /* TODO(brett) dragging.
      ContinueDrag(this, *event);
      */
      break;

    case ui::ET_GESTURE_TAP_DOWN:
      /* TODO(brettw) dragging.
      EndDrag(END_DRAG_CANCEL);
      */
      break;

    case ui::ET_GESTURE_TAP: {
      /* TODO(brettw) activation UMA.
      const int active_index = GetActiveIndex();
      DCHECK_NE(-1, active_index);
      TabExperimental* active_tab = tab_at(active_index);
      TouchUMA::GestureActionType action = TouchUMA::GESTURE_TABNOSWITCH_TAP;
      if (active_tab->tab_activated_with_last_tap_down())
        action = TouchUMA::GESTURE_TABSWITCH_TAP;
      TouchUMA::RecordGestureAction(action);
      */
      break;
    }

    default:
      break;
  }
  event->SetHandled();
}

views::View* TabStripExperimental::TargetForRect(views::View* root,
                                                 const gfx::Rect& rect) {
  CHECK_EQ(root, this);

  if (!views::UsePointBasedTargeting(rect))
    return views::ViewTargeterDelegate::TargetForRect(root, rect);
  const gfx::Point point(rect.CenterPoint());

  // Return any view that isn't a TabExperimental or this TabStrip
  // immediately. We don't want to interfere.
  views::View* v = views::ViewTargeterDelegate::TargetForRect(root, rect);
  if (v && v != this &&
      strcmp(v->GetClassName(), TabExperimental::kViewClassName))
    return v;

  views::View* tab = FindTabHitByPoint(point);
  if (tab)
    return tab;
  return this;
}

TabExperimental* TabStripExperimental::TabForData(
    const TabDataExperimental* data) {
  auto found = tabs_.find(data);
  if (found == tabs_.end())
    return nullptr;
  return found->second;
}

void TabStripExperimental::InvalidateViewOrder() {
  view_order_.clear();
}

void TabStripExperimental::EnsureViewOrderUpToDate() const {
  if (!view_order_.empty())
    return;

  view_order_.reserve(tabs_.size());

  for (const auto& top_data : model_->top_level_tabs()) {
    // Add children. This assumes a two-level hierarchy. If more nesting is
    // required, this will need to be recursive.
    for (const auto& inner_data : top_data->children()) {
      const auto inner_found = tabs_.find(inner_data.get());
      if (inner_found == tabs_.end()) {
        NOTREACHED();
      } else {
        inner_found->second->set_view_order(view_order_.size());
        view_order_.push_back(inner_found->second);
      }
    }

    // The group itself goes below the child tabs.
    const auto top_found = tabs_.find(top_data.get());
    if (top_found == tabs_.end()) {
      NOTREACHED();
    } else {
      top_found->second->set_view_order(view_order_.size());
      view_order_.push_back(top_found->second);
    }
  }

  // Put the closing tabs at the back.
  // TODO(brettw) this isn't right, they should go in the order they were
  // originally in the tab strip. The regular tab strip does a lot of index
  // tracking for this purpose.
  for (TabExperimental* tab : closing_tabs_) {
    tab->set_view_order(view_order_.size());
    view_order_.push_back(tab);
  }
}
