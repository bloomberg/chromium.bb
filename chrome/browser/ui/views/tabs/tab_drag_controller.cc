// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_drag_controller.h"

#include <math.h>
#include <set>

#include "base/auto_reset.h"
#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/stl_util.h"
#include "build/build_config.h"
#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/ash_util.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/tabs/tab_strip_model_delegate.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/stacked_tab_strip_layout.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/window_finder.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "extensions/browser/extension_function_dispatcher.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/events/event_constants.h"
#include "ui/events/gestures/gesture_recognizer.h"
#include "ui/gfx/geometry/point_conversions.h"
#include "ui/views/event_monitor.h"
#include "ui/views/view_tracker.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

#if defined(OS_CHROMEOS)
#include "ash/public/cpp/window_properties.h"               // nogncheck
#include "ash/public/interfaces/window_state_type.mojom.h"  // nogncheck
#include "chrome/browser/ui/ash/tablet_mode_client.h"
#include "ui/wm/core/coordinate_conversion.h"
#endif

#if defined(USE_AURA)
#include "ui/aura/env.h"  // nogncheck
#include "ui/aura/window.h"  // nogncheck
#include "ui/wm/core/window_modality_controller.h"  // nogncheck
#endif

using content::OpenURLParams;
using content::WebContents;

// If non-null there is a drag underway.
static TabDragController* instance_ = NULL;

namespace {

// Delay, in ms, during dragging before we bring a window to front.
const int kBringToFrontDelay = 750;

// Initial delay before moving tabs when the dragged tab is close to the edge of
// the stacked tabs.
const int kMoveAttachedInitialDelay = 600;

// Delay for moving tabs after the initial delay has passed.
const int kMoveAttachedSubsequentDelay = 300;

const int kHorizontalMoveThreshold = 16;  // DIPs.

// The inset within the first dragged tab to use when calculating the "drag
// insertion point".  If we simply used the x-coordinate of the tab, we'd be
// calculating based on a point well before where the user considers the tab to
// "be".  The value here is chosen to "feel good" based on the widths of the tab
// images and the tab overlap.
//
// Note that this must return a value smaller than the midpoint of any tab's
// width, or else the user won't be able to drag a tab to the left of the first
// tab in the strip.
//
// TODO(pkasting): Maybe this should use Tab::GetOverlap() instead?
const int kLeadingWidthForDrag = 16;

// Distance from the next/previous stacked before before we consider the tab
// close enough to trigger moving.
const int kStackedDistance = 36;

// A dragged window is forced to be a bit smaller than maximized bounds during a
// drag. This prevents the dragged browser widget from getting maximized at
// creation and makes it easier to drag tabs out of a restored window that had
// maximized size.
const int kMaximizedWindowInset = 10;  // DIPs.

#if defined(OS_CHROMEOS)
// Returns true if |tab_strip| browser window is snapped.
bool IsSnapped(const TabStrip* tab_strip) {
  DCHECK(tab_strip);
  ash::mojom::WindowStateType type =
      tab_strip->GetWidget()->GetNativeWindow()->GetProperty(
          ash::kWindowStateTypeKey);
  return type == ash::mojom::WindowStateType::LEFT_SNAPPED ||
         type == ash::mojom::WindowStateType::RIGHT_SNAPPED;
}
#else
bool IsSnapped(const TabStrip* tab_strip) {
  return false;
}
#endif

#if defined(USE_AURA)
gfx::NativeWindow GetModalTransient(gfx::NativeWindow window) {
  return wm::GetModalTransient(window);
}
#else
gfx::NativeWindow GetModalTransient(gfx::NativeWindow window) {
  NOTIMPLEMENTED();
  return NULL;
}
#endif

// Returns true if |bounds| contains the y-coordinate |y|. The y-coordinate
// of |bounds| is adjusted by |vertical_adjustment|.
bool DoesRectContainVerticalPointExpanded(
    const gfx::Rect& bounds,
    int vertical_adjustment,
    int y) {
  int upper_threshold = bounds.bottom() + vertical_adjustment;
  int lower_threshold = bounds.y() - vertical_adjustment;
  return y >= lower_threshold && y <= upper_threshold;
}

// Adds |x_offset| to all the rectangles in |rects|.
void OffsetX(int x_offset, std::vector<gfx::Rect>* rects) {
  if (x_offset == 0)
    return;

  for (size_t i = 0; i < rects->size(); ++i)
    (*rects)[i].set_x((*rects)[i].x() + x_offset);
}

// EscapeTracker installs an event monitor and runs a callback when it receives
// the escape key.
class EscapeTracker : public ui::EventHandler {
 public:
  explicit EscapeTracker(const base::Closure& callback)
      : escape_callback_(callback),
        event_monitor_(views::EventMonitor::CreateApplicationMonitor(this)) {
  }

 private:
  // ui::EventHandler:
  void OnKeyEvent(ui::KeyEvent* key) override {
    if (key->type() == ui::ET_KEY_PRESSED &&
        key->key_code() == ui::VKEY_ESCAPE) {
      escape_callback_.Run();
    }
  }

  base::Closure escape_callback_;
  std::unique_ptr<views::EventMonitor> event_monitor_;

  DISALLOW_COPY_AND_ASSIGN(EscapeTracker);
};

}  // namespace

TabDragController::TabDragData::TabDragData()
    : contents(NULL),
      source_model_index(-1),
      attached_tab(NULL),
      pinned(false) {
}

TabDragController::TabDragData::~TabDragData() {
}

///////////////////////////////////////////////////////////////////////////////
// TabDragController, public:

// static
const int TabDragController::kTouchVerticalDetachMagnetism = 50;

// static
const int TabDragController::kVerticalDetachMagnetism = 15;

TabDragController::TabDragController()
    : event_source_(EVENT_SOURCE_MOUSE),
      source_tabstrip_(NULL),
      attached_tabstrip_(NULL),
      can_release_capture_(true),
      offset_to_width_ratio_(0),
      old_focused_view_tracker_(base::MakeUnique<views::ViewTracker>()),
      last_move_screen_loc_(0),
      started_drag_(false),
      active_(true),
      source_tab_index_(std::numeric_limits<size_t>::max()),
      initial_move_(true),
      detach_behavior_(DETACHABLE),
      move_behavior_(REORDER),
      mouse_move_direction_(0),
      is_dragging_window_(false),
      is_dragging_new_browser_(false),
      was_source_maximized_(false),
      was_source_fullscreen_(false),
      did_restore_window_(false),
      end_run_loop_behavior_(END_RUN_LOOP_STOP_DRAGGING),
      waiting_for_run_loop_to_exit_(false),
      tab_strip_to_attach_to_after_exit_(NULL),
      move_loop_widget_(NULL),
      is_mutating_(false),
      attach_x_(-1),
      attach_index_(-1),
      window_finder_(base::MakeUnique<WindowFinder>()),
      weak_factory_(this) {
  instance_ = this;
}

TabDragController::~TabDragController() {
  if (instance_ == this)
    instance_ = NULL;

  if (move_loop_widget_) {
    if (added_observer_to_move_loop_widget_)
      move_loop_widget_->RemoveObserver(this);
  }

  if (source_tabstrip_)
    GetModel(source_tabstrip_)->RemoveObserver(this);

  if (event_source_ == EVENT_SOURCE_TOUCH) {
    TabStrip* capture_tabstrip = attached_tabstrip_ ?
        attached_tabstrip_ : source_tabstrip_;
    capture_tabstrip->GetWidget()->ReleaseCapture();
  }
}

void TabDragController::Init(
    TabStrip* source_tabstrip,
    Tab* source_tab,
    const std::vector<Tab*>& tabs,
    const gfx::Point& mouse_offset,
    int source_tab_offset,
    const ui::ListSelectionModel& initial_selection_model,
    MoveBehavior move_behavior,
    EventSource event_source) {
  DCHECK(!tabs.empty());
  DCHECK(base::ContainsValue(tabs, source_tab));
  source_tabstrip_ = source_tabstrip;
  was_source_maximized_ = source_tabstrip->GetWidget()->IsMaximized();
  was_source_fullscreen_ = source_tabstrip->GetWidget()->IsFullscreen();
  // Do not release capture when transferring capture between widgets on:
  // - Desktop Linux
  //     Mouse capture is not synchronous on desktop Linux. Chrome makes
  //     transferring capture between widgets without releasing capture appear
  //     synchronous on desktop Linux, so use that.
  // - Chrome OS
  //     Releasing capture on Ash cancels gestures so avoid it.
#if defined(OS_LINUX)
  can_release_capture_ = false;
#endif
  start_point_in_screen_ = gfx::Point(source_tab_offset, mouse_offset.y());
  views::View::ConvertPointToScreen(source_tab, &start_point_in_screen_);
  event_source_ = event_source;
  mouse_offset_ = mouse_offset;
  move_behavior_ = move_behavior;
  last_point_in_screen_ = start_point_in_screen_;
  last_move_screen_loc_ = start_point_in_screen_.x();
  initial_tab_positions_ = source_tabstrip->GetTabXCoordinates();

  GetModel(source_tabstrip_)->AddObserver(this);

  drag_data_.resize(tabs.size());
  for (size_t i = 0; i < tabs.size(); ++i)
    InitTabDragData(tabs[i], &(drag_data_[i]));
  source_tab_index_ =
      std::find(tabs.begin(), tabs.end(), source_tab) - tabs.begin();

  // Listen for Esc key presses.
  escape_tracker_.reset(
      new EscapeTracker(base::Bind(&TabDragController::EndDrag,
                                   weak_factory_.GetWeakPtr(),
                                   END_DRAG_CANCEL)));

  if (source_tab->width() > 0) {
    offset_to_width_ratio_ = static_cast<float>(
        source_tab->GetMirroredXInView(source_tab_offset)) /
        static_cast<float>(source_tab->width());
  }
  InitWindowCreatePoint();
  initial_selection_model_.Copy(initial_selection_model);

  // Gestures don't automatically do a capture. We don't allow multiple drags at
  // the same time, so we explicitly capture.
  if (event_source == EVENT_SOURCE_TOUCH)
    source_tabstrip_->GetWidget()->SetCapture(source_tabstrip_);

#if defined(OS_CHROMEOS)
  if (TabletModeClient::Get()->tablet_mode_enabled())
    detach_behavior_ = NOT_DETACHABLE;
#endif
}

// static
bool TabDragController::IsAttachedTo(const TabStrip* tab_strip) {
  return (instance_ && instance_->active() &&
          instance_->attached_tabstrip() == tab_strip);
}

// static
bool TabDragController::IsActive() {
  return instance_ && instance_->active();
}

void TabDragController::SetMoveBehavior(MoveBehavior behavior) {
  if (started_drag())
    return;

  move_behavior_ = behavior;
}

bool TabDragController::IsDraggingTab(content::WebContents* contents) {
  for (auto drag_data : drag_data_) {
    if (drag_data.contents == contents)
      return true;
  }
  return false;
}

void TabDragController::Drag(const gfx::Point& point_in_screen) {
  TRACE_EVENT1("views", "TabDragController::Drag",
               "point_in_screen", point_in_screen.ToString());

  bring_to_front_timer_.Stop();
  move_stacked_timer_.Stop();

  if (waiting_for_run_loop_to_exit_)
    return;

  if (!started_drag_) {
    if (!CanStartDrag(point_in_screen))
      return;  // User hasn't dragged far enough yet.

    // On windows SaveFocus() may trigger a capture lost, which destroys us.
    {
      base::WeakPtr<TabDragController> ref(weak_factory_.GetWeakPtr());
      SaveFocus();
      if (!ref)
        return;
    }
    started_drag_ = true;
    Attach(source_tabstrip_, gfx::Point());
    if (static_cast<int>(drag_data_.size()) ==
        GetModel(source_tabstrip_)->count()) {
      if (was_source_maximized_ || was_source_fullscreen_) {
        did_restore_window_ = true;
        // When all tabs in a maximized browser are dragged the browser gets
        // restored during the drag and maximized back when the drag ends.
        views::Widget* widget = GetAttachedBrowserWidget();
        const int last_tabstrip_width = attached_tabstrip_->GetTabAreaWidth();
        std::vector<gfx::Rect> drag_bounds = CalculateBoundsForDraggedTabs();
        OffsetX(GetAttachedDragPoint(point_in_screen).x(), &drag_bounds);
        gfx::Rect new_bounds(CalculateDraggedBrowserBounds(source_tabstrip_,
                                                           point_in_screen,
                                                           &drag_bounds));
        new_bounds.Offset(-widget->GetRestoredBounds().x() +
                          point_in_screen.x() -
                          mouse_offset_.x(), 0);
        widget->SetVisibilityChangedAnimationsEnabled(false);
        widget->Restore();
        widget->SetBounds(new_bounds);
        AdjustBrowserAndTabBoundsForDrag(last_tabstrip_width,
                                         point_in_screen,
                                         &drag_bounds);
        widget->SetVisibilityChangedAnimationsEnabled(true);
      } else {
        // The user has to move the mouse some amount of pixels before the drag
        // starts. Offset the window by this amount so that the relative offset
        // of the initial location is consistent. See crbug.com/518740
        views::Widget* widget = GetAttachedBrowserWidget();
        gfx::Rect bounds = widget->GetWindowBoundsInScreen();
        bounds.Offset(point_in_screen.x() - start_point_in_screen_.x(),
                      point_in_screen.y() - start_point_in_screen_.y());
        widget->SetBounds(bounds);
      }
      RunMoveLoop(GetWindowOffset(point_in_screen));
      return;
    }
  }

  if (ContinueDragging(point_in_screen) == Liveness::DELETED)
    return;
}

void TabDragController::EndDrag(EndDragReason reason) {
  TRACE_EVENT0("views", "TabDragController::EndDrag");

  // If we're dragging a window ignore capture lost since it'll ultimately
  // trigger the move loop to end and we'll revert the drag when RunMoveLoop()
  // finishes.
  if (reason == END_DRAG_CAPTURE_LOST && is_dragging_window_)
    return;
  EndDragImpl(reason != END_DRAG_COMPLETE && source_tabstrip_ ?
              CANCELED : NORMAL);
}

void TabDragController::InitTabDragData(Tab* tab,
                                        TabDragData* drag_data) {
  TRACE_EVENT0("views", "TabDragController::InitTabDragData");
  drag_data->source_model_index =
      source_tabstrip_->GetModelIndexOfTab(tab);
  drag_data->contents = GetModel(source_tabstrip_)->GetWebContentsAt(
      drag_data->source_model_index);
  drag_data->pinned = source_tabstrip_->IsTabPinned(tab);
}

void TabDragController::OnWidgetBoundsChanged(views::Widget* widget,
                                              const gfx::Rect& new_bounds) {
  TRACE_EVENT1("views", "TabDragController::OnWidgetBoundsChanged",
               "new_bounds", new_bounds.ToString());

  Drag(GetCursorScreenPoint());
}

void TabDragController::TabStripEmpty() {
  GetModel(source_tabstrip_)->RemoveObserver(this);
  // NULL out source_tabstrip_ so that we don't attempt to add back to it (in
  // the case of a revert).
  source_tabstrip_ = NULL;
}

///////////////////////////////////////////////////////////////////////////////
// TabDragController, private:

void TabDragController::InitWindowCreatePoint() {
  // window_create_point_ is only used in CompleteDrag() (through
  // GetWindowCreatePoint() to get the start point of the docked window) when
  // the attached_tabstrip_ is NULL and all the window's related bound
  // information are obtained from source_tabstrip_. So, we need to get the
  // first_tab based on source_tabstrip_, not attached_tabstrip_. Otherwise,
  // the window_create_point_ is not in the correct coordinate system. Please
  // refer to http://crbug.com/6223 comment #15 for detailed information.
  views::View* first_tab = source_tabstrip_->tab_at(0);
  views::View::ConvertPointToWidget(first_tab, &first_source_tab_point_);
  window_create_point_ = first_source_tab_point_;
  window_create_point_.Offset(mouse_offset_.x(), mouse_offset_.y());
}

gfx::Point TabDragController::GetWindowCreatePoint(
    const gfx::Point& origin) const {
  // If the cursor is outside the monitor area, move it inside. For example,
  // dropping a tab onto the task bar on Windows produces this situation.
  gfx::Rect work_area =
      display::Screen::GetScreen()->GetDisplayNearestPoint(origin).work_area();
  gfx::Point create_point(origin);
  if (!work_area.IsEmpty()) {
    if (create_point.x() < work_area.x())
      create_point.set_x(work_area.x());
    else if (create_point.x() > work_area.right())
      create_point.set_x(work_area.right());
    if (create_point.y() < work_area.y())
      create_point.set_y(work_area.y());
    else if (create_point.y() > work_area.bottom())
      create_point.set_y(work_area.bottom());
  }
  return gfx::Point(create_point.x() - window_create_point_.x(),
                    create_point.y() - window_create_point_.y());
}

void TabDragController::SaveFocus() {
  DCHECK(source_tabstrip_);
  old_focused_view_tracker_->SetView(
      source_tabstrip_->GetFocusManager()->GetFocusedView());
  source_tabstrip_->GetFocusManager()->SetFocusedView(source_tabstrip_);
  // WARNING: we may have been deleted.
}

void TabDragController::RestoreFocus() {
  if (attached_tabstrip_ != source_tabstrip_) {
    if (is_dragging_new_browser_) {
      content::WebContents* active_contents = source_dragged_contents();
      if (active_contents && !active_contents->FocusLocationBarByDefault())
        active_contents->Focus();
    }
    return;
  }
  views::View* old_focused_view = old_focused_view_tracker_->view();
  if (!old_focused_view)
    return;
  old_focused_view->GetFocusManager()->SetFocusedView(old_focused_view);
}

bool TabDragController::CanStartDrag(const gfx::Point& point_in_screen) const {
  // Determine if the mouse has moved beyond a minimum elasticity distance in
  // any direction from the starting point.
  static const int kMinimumDragDistance = 10;
  int x_offset = abs(point_in_screen.x() - start_point_in_screen_.x());
  int y_offset = abs(point_in_screen.y() - start_point_in_screen_.y());
  return sqrt(pow(static_cast<float>(x_offset), 2) +
              pow(static_cast<float>(y_offset), 2)) > kMinimumDragDistance;
}

TabDragController::Liveness TabDragController::ContinueDragging(
    const gfx::Point& point_in_screen) {
  TRACE_EVENT1("views", "TabDragController::ContinueDragging",
               "point_in_screen", point_in_screen.ToString());

  DCHECK(attached_tabstrip_);

  TabStrip* target_tabstrip = source_tabstrip_;
  if (detach_behavior_ == DETACHABLE &&
      GetTargetTabStripForPoint(point_in_screen, &target_tabstrip) ==
          Liveness::DELETED) {
    return Liveness::DELETED;
  }
  bool tab_strip_changed = (target_tabstrip != attached_tabstrip_);

  if (attached_tabstrip_) {
    int move_delta = point_in_screen.x() - last_point_in_screen_.x();
    if (move_delta > 0)
      mouse_move_direction_ |= kMovedMouseRight;
    else if (move_delta < 0)
      mouse_move_direction_ |= kMovedMouseLeft;
  }
  last_point_in_screen_ = point_in_screen;

  if (tab_strip_changed) {
    is_dragging_new_browser_ = false;
    did_restore_window_ = false;
    if (DragBrowserToNewTabStrip(target_tabstrip, point_in_screen) ==
        DRAG_BROWSER_RESULT_STOP) {
      return Liveness::ALIVE;
    }
  }
  if (is_dragging_window_) {
    static_cast<base::Timer*>(&bring_to_front_timer_)->Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(kBringToFrontDelay),
        base::Bind(&TabDragController::BringWindowUnderPointToFront,
                   base::Unretained(this), point_in_screen));
  }

  if (!is_dragging_window_ && attached_tabstrip_) {
    if (move_only()) {
      DragActiveTabStacked(point_in_screen);
    } else {
      MoveAttached(point_in_screen);
      if (tab_strip_changed) {
        // Move the corresponding window to the front. We do this after the
        // move as on windows activate triggers a synchronous paint.
        attached_tabstrip_->GetWidget()->Activate();
      }
    }
  }
  return Liveness::ALIVE;
}

TabDragController::DragBrowserResultType
TabDragController::DragBrowserToNewTabStrip(
    TabStrip* target_tabstrip,
    const gfx::Point& point_in_screen) {
  TRACE_EVENT1("views", "TabDragController::DragBrowserToNewTabStrip",
               "point_in_screen", point_in_screen.ToString());

  if (!target_tabstrip) {
    DetachIntoNewBrowserAndRunMoveLoop(point_in_screen);
    return DRAG_BROWSER_RESULT_STOP;
  }

#if defined(USE_AURA)
  // Only Aura windows are gesture consumers.
  ui::GestureRecognizer::Get()->TransferEventsTo(
      GetAttachedBrowserWidget()->GetNativeView(),
      target_tabstrip->GetWidget()->GetNativeView(),
      ui::GestureRecognizer::ShouldCancelTouches::DontCancel);
#endif

  if (is_dragging_window_) {
    // ReleaseCapture() is going to result in calling back to us (because it
    // results in a move). That'll cause all sorts of problems.  Reset the
    // observer so we don't get notified and process the event.
#if defined(OS_CHROMEOS)
    move_loop_widget_->RemoveObserver(this);
    move_loop_widget_ = nullptr;
#endif  // OS_CHROMEOS
    views::Widget* browser_widget = GetAttachedBrowserWidget();
    // Need to release the drag controller before starting the move loop as it's
    // going to trigger capture lost, which cancels drag.
    attached_tabstrip_->ReleaseDragController();
    target_tabstrip->OwnDragController(this);
    // Disable animations so that we don't see a close animation on aero.
    browser_widget->SetVisibilityChangedAnimationsEnabled(false);
    if (can_release_capture_)
      browser_widget->ReleaseCapture();
    else
      target_tabstrip->GetWidget()->SetCapture(attached_tabstrip_);

#if !defined(OS_LINUX)
    // EndMoveLoop is going to snap the window back to its original location.
    // Hide it so users don't see this. Hiding a window in Linux aura causes
    // it to lose capture so skip it.
    browser_widget->Hide();
#endif
    browser_widget->EndMoveLoop();

    // Ideally we would always swap the tabs now, but on non-ash Windows, it
    // seems that running the move loop implicitly activates the window when
    // done, leading to all sorts of flicker. So, on non-ash Windows, instead
    // we process the move after the loop completes. But on chromeos, we can
    // do tab swapping now to avoid the tab flashing issue
    // (crbug.com/116329).
    if (can_release_capture_) {
      tab_strip_to_attach_to_after_exit_ = target_tabstrip;
    } else {
      is_dragging_window_ = false;
      Detach(DONT_RELEASE_CAPTURE);
      Attach(target_tabstrip, point_in_screen);
      // Move the tabs into position.
      MoveAttached(point_in_screen);
      attached_tabstrip_->GetWidget()->Activate();
    }

    waiting_for_run_loop_to_exit_ = true;
    end_run_loop_behavior_ = END_RUN_LOOP_CONTINUE_DRAGGING;
    return DRAG_BROWSER_RESULT_STOP;
  }
  Detach(DONT_RELEASE_CAPTURE);
  Attach(target_tabstrip, point_in_screen);
  return DRAG_BROWSER_RESULT_CONTINUE;
}

void TabDragController::DragActiveTabStacked(
    const gfx::Point& point_in_screen) {
  if (attached_tabstrip_->tab_count() !=
      static_cast<int>(initial_tab_positions_.size()))
    return;  // TODO: should cancel drag if this happens.

  int delta = point_in_screen.x() - start_point_in_screen_.x();
  attached_tabstrip_->DragActiveTab(initial_tab_positions_, delta);
}

void TabDragController::MoveAttachedToNextStackedIndex(
    const gfx::Point& point_in_screen) {
  int index = attached_tabstrip_->touch_layout_->active_index();
  if (index + 1 >= attached_tabstrip_->tab_count())
    return;

  GetModel(attached_tabstrip_)->MoveSelectedTabsTo(index + 1);
  StartMoveStackedTimerIfNecessary(point_in_screen,
                                   kMoveAttachedSubsequentDelay);
}

void TabDragController::MoveAttachedToPreviousStackedIndex(
    const gfx::Point& point_in_screen) {
  int index = attached_tabstrip_->touch_layout_->active_index();
  if (index <= attached_tabstrip_->GetPinnedTabCount())
    return;

  GetModel(attached_tabstrip_)->MoveSelectedTabsTo(index - 1);
  StartMoveStackedTimerIfNecessary(point_in_screen,
                                   kMoveAttachedSubsequentDelay);
}

void TabDragController::MoveAttached(const gfx::Point& point_in_screen) {
  DCHECK(attached_tabstrip_);
  DCHECK(!is_dragging_window_);

  gfx::Point dragged_view_point = GetAttachedDragPoint(point_in_screen);

  // Determine the horizontal move threshold. This is dependent on the width
  // of tabs. The smaller the tabs compared to the standard size, the smaller
  // the threshold.
  int threshold = kHorizontalMoveThreshold;
  if (!attached_tabstrip_->touch_layout_.get()) {
    double ratio =
        static_cast<double>(attached_tabstrip_->current_inactive_width()) /
        Tab::GetStandardSize().width();
    threshold = static_cast<int>(ratio * kHorizontalMoveThreshold);
  }
  // else case: touch tabs never shrink.

  std::vector<Tab*> tabs(drag_data_.size());
  for (size_t i = 0; i < drag_data_.size(); ++i)
    tabs[i] = drag_data_[i].attached_tab;

  bool did_layout = false;
  // Update the model, moving the WebContents from one index to another. Do this
  // only if we have moved a minimum distance since the last reorder (to prevent
  // jitter) or if this the first move and the tabs are not consecutive.
  if ((abs(point_in_screen.x() - last_move_screen_loc_) > threshold ||
        (initial_move_ && !AreTabsConsecutive()))) {
    TabStripModel* attached_model = GetModel(attached_tabstrip_);
    int to_index = GetInsertionIndexForDraggedBounds(
        GetDraggedViewTabStripBounds(dragged_view_point));
    bool do_move = true;
    // While dragging within a tabstrip the expectation is the insertion index
    // is based on the left edge of the tabs being dragged. OTOH when dragging
    // into a new tabstrip (attaching) the expectation is the insertion index is
    // based on the cursor. This proves problematic as insertion may change the
    // size of the tabs, resulting in the index calculated before the insert
    // differing from the index calculated after the insert. To alleviate this
    // the index is chosen before insertion, and subsequently a new index is
    // only used once the mouse moves enough such that the index changes based
    // on the direction the mouse moved relative to |attach_x_| (smaller
    // x-coordinate should yield a smaller index or larger x-coordinate yields a
    // larger index).
    if (attach_index_ != -1) {
      gfx::Point tab_strip_point(point_in_screen);
      views::View::ConvertPointFromScreen(attached_tabstrip_, &tab_strip_point);
      const int new_x =
          attached_tabstrip_->GetMirroredXInView(tab_strip_point.x());
      if (new_x < attach_x_)
        to_index = std::min(to_index, attach_index_);
      else
        to_index = std::max(to_index, attach_index_);
      if (to_index != attach_index_)
        attach_index_ = -1;  // Once a valid move is detected, don't constrain.
      else
        do_move = false;
    }
    if (do_move) {
      WebContents* last_contents = drag_data_.back().contents;
      int index_of_last_item =
          attached_model->GetIndexOfWebContents(last_contents);
      if (initial_move_) {
        // TabStrip determines if the tabs needs to be animated based on model
        // position. This means we need to invoke LayoutDraggedTabsAt before
        // changing the model.
        attached_tabstrip_->LayoutDraggedTabsAt(
            tabs, source_tab_drag_data()->attached_tab, dragged_view_point,
            initial_move_);
        did_layout = true;
      }
      attached_model->MoveSelectedTabsTo(to_index);

      // Move may do nothing in certain situations (such as when dragging pinned
      // tabs). Make sure the tabstrip actually changed before updating
      // last_move_screen_loc_.
      if (index_of_last_item !=
          attached_model->GetIndexOfWebContents(last_contents)) {
        last_move_screen_loc_ = point_in_screen.x();
      }
    }
  }

  if (!did_layout) {
    attached_tabstrip_->LayoutDraggedTabsAt(
        tabs, source_tab_drag_data()->attached_tab, dragged_view_point,
        initial_move_);
  }

  StartMoveStackedTimerIfNecessary(point_in_screen, kMoveAttachedInitialDelay);

  initial_move_ = false;
}

void TabDragController::StartMoveStackedTimerIfNecessary(
    const gfx::Point& point_in_screen,
    int delay_ms) {
  DCHECK(attached_tabstrip_);

  StackedTabStripLayout* touch_layout = attached_tabstrip_->touch_layout_.get();
  if (!touch_layout)
    return;

  gfx::Point dragged_view_point = GetAttachedDragPoint(point_in_screen);
  gfx::Rect bounds = GetDraggedViewTabStripBounds(dragged_view_point);
  int index = touch_layout->active_index();
  if (ShouldDragToNextStackedTab(bounds, index)) {
    static_cast<base::Timer*>(&move_stacked_timer_)->Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(delay_ms),
        base::Bind(&TabDragController::MoveAttachedToNextStackedIndex,
                   base::Unretained(this), point_in_screen));
  } else if (ShouldDragToPreviousStackedTab(bounds, index)) {
    static_cast<base::Timer*>(&move_stacked_timer_)->Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(delay_ms),
        base::Bind(&TabDragController::MoveAttachedToPreviousStackedIndex,
                   base::Unretained(this), point_in_screen));
  }
}

TabDragController::DetachPosition TabDragController::GetDetachPosition(
    const gfx::Point& point_in_screen) {
  DCHECK(attached_tabstrip_);
  gfx::Point attached_point(point_in_screen);
  views::View::ConvertPointFromScreen(attached_tabstrip_, &attached_point);
  if (attached_point.x() < 0)
    return DETACH_BEFORE;
  if (attached_point.x() >= attached_tabstrip_->width())
    return DETACH_AFTER;
  return DETACH_ABOVE_OR_BELOW;
}

TabDragController::Liveness TabDragController::GetTargetTabStripForPoint(
    const gfx::Point& point_in_screen,
    TabStrip** tab_strip) {
  *tab_strip = nullptr;
  TRACE_EVENT1("views", "TabDragController::GetTargetTabStripForPoint",
               "point_in_screen", point_in_screen.ToString());

  if (move_only() && attached_tabstrip_) {
    // move_only() is intended for touch, in which case we only want to detach
    // if the touch point moves significantly in the vertical distance.
    gfx::Rect tabstrip_bounds = GetViewScreenBounds(attached_tabstrip_);
    if (DoesRectContainVerticalPointExpanded(tabstrip_bounds,
                                             kTouchVerticalDetachMagnetism,
                                             point_in_screen.y())) {
      *tab_strip = attached_tabstrip_;
      return Liveness::ALIVE;
    }
  }
  gfx::NativeWindow local_window;
  const Liveness state = GetLocalProcessWindow(
      point_in_screen, is_dragging_window_, &local_window);
  if (state == Liveness::DELETED)
    return Liveness::DELETED;

  // Do not allow dragging into a window with a modal dialog, it causes a weird
  // behavior.  See crbug.com/336691
  if (!GetModalTransient(local_window)) {
    TabStrip* result = GetTabStripForWindow(local_window);
    if (result && DoesTabStripContain(result, point_in_screen)) {
      *tab_strip = result;
      return Liveness::ALIVE;
    }
  }

  *tab_strip = is_dragging_window_ ? attached_tabstrip_ : nullptr;
  return Liveness::ALIVE;
}

TabStrip* TabDragController::GetTabStripForWindow(gfx::NativeWindow window) {
  if (!window)
    return NULL;
  BrowserView* browser_view =
      BrowserView::GetBrowserViewForNativeWindow(window);
  // We don't allow drops on windows that don't have tabstrips.
  if (!browser_view ||
      !browser_view->browser()->SupportsWindowFeature(
          Browser::FEATURE_TABSTRIP))
    return NULL;

  TabStrip* other_tabstrip = browser_view->tabstrip();
  TabStrip* tab_strip =
      attached_tabstrip_ ? attached_tabstrip_ : source_tabstrip_;
  DCHECK(tab_strip);

  return other_tabstrip->controller()->IsCompatibleWith(tab_strip) ?
      other_tabstrip : NULL;
}

bool TabDragController::DoesTabStripContain(
    TabStrip* tabstrip,
    const gfx::Point& point_in_screen) const {
  // Make sure the specified screen point is actually within the bounds of the
  // specified tabstrip...
  gfx::Rect tabstrip_bounds = GetViewScreenBounds(tabstrip);
  return point_in_screen.x() < tabstrip_bounds.right() &&
      point_in_screen.x() >= tabstrip_bounds.x() &&
      DoesRectContainVerticalPointExpanded(tabstrip_bounds,
                                           kVerticalDetachMagnetism,
                                           point_in_screen.y());
}

void TabDragController::Attach(TabStrip* attached_tabstrip,
                               const gfx::Point& point_in_screen) {
  TRACE_EVENT1("views", "TabDragController::Attach",
               "point_in_screen", point_in_screen.ToString());

  DCHECK(!attached_tabstrip_);  // We should already have detached by the time
                                // we get here.

  attached_tabstrip_ = attached_tabstrip;

  std::vector<Tab*> tabs =
      GetTabsMatchingDraggedContents(attached_tabstrip_);

  if (tabs.empty()) {
    // Transitioning from detached to attached to a new tabstrip. Add tabs to
    // the new model.

    selection_model_before_attach_.Copy(attached_tabstrip->GetSelectionModel());

    // Inserting counts as a move. We don't want the tabs to jitter when the
    // user moves the tab immediately after attaching it.
    last_move_screen_loc_ = point_in_screen.x();

    // Figure out where to insert the tab based on the bounds of the dragged
    // representation and the ideal bounds of the other Tabs already in the
    // strip. ("ideal bounds" are stable even if the Tabs' actual bounds are
    // changing due to animation).
    gfx::Point tab_strip_point(point_in_screen);
    views::View::ConvertPointFromScreen(attached_tabstrip_, &tab_strip_point);
    tab_strip_point.set_x(
        attached_tabstrip_->GetMirroredXInView(tab_strip_point.x()));
    tab_strip_point.Offset(0, -mouse_offset_.y());
    int index = GetInsertionIndexForDraggedBounds(
        GetDraggedViewTabStripBounds(tab_strip_point));
    attach_index_ = index;
    attach_x_ = tab_strip_point.x();
    base::AutoReset<bool> setter(&is_mutating_, true);
    for (size_t i = 0; i < drag_data_.size(); ++i) {
      int add_types = TabStripModel::ADD_NONE;
      if (attached_tabstrip_->touch_layout_.get()) {
        // StackedTabStripLayout positions relative to the active tab, if we
        // don't add the tab as active things bounce around.
        DCHECK_EQ(1u, drag_data_.size());
        add_types |= TabStripModel::ADD_ACTIVE;
      }
      if (drag_data_[i].pinned)
        add_types |= TabStripModel::ADD_PINNED;
      GetModel(attached_tabstrip_)->InsertWebContentsAt(
          index + i, drag_data_[i].contents, add_types);
    }

    tabs = GetTabsMatchingDraggedContents(attached_tabstrip_);
  }
  DCHECK_EQ(tabs.size(), drag_data_.size());
  for (size_t i = 0; i < drag_data_.size(); ++i)
    drag_data_[i].attached_tab = tabs[i];

  attached_tabstrip_->StartedDraggingTabs(tabs);

  ResetSelection(GetModel(attached_tabstrip_));

  // The size of the dragged tab may have changed. Adjust the x offset so that
  // ratio of mouse_offset_ to original width is maintained.
  std::vector<Tab*> tabs_to_source(tabs);
  tabs_to_source.erase(tabs_to_source.begin() + source_tab_index_ + 1,
                       tabs_to_source.end());
  int new_x = attached_tabstrip_->GetSizeNeededForTabs(tabs_to_source) -
      tabs[source_tab_index_]->width() +
      static_cast<int>(offset_to_width_ratio_ *
                       tabs[source_tab_index_]->width());
  mouse_offset_.set_x(new_x);

  // Transfer ownership of us to the new tabstrip as well as making sure the
  // window has capture. This is important so that if activation changes the
  // drag isn't prematurely canceled.
  attached_tabstrip_->GetWidget()->SetCapture(attached_tabstrip_);
  attached_tabstrip_->OwnDragController(this);
}

void TabDragController::Detach(ReleaseCapture release_capture) {
  TRACE_EVENT1("views", "TabDragController::Detach",
               "release_capture", release_capture);

  attach_index_ = -1;

  // When the user detaches we assume they want to reorder.
  move_behavior_ = REORDER;

  // Release ownership of the drag controller and mouse capture. When we
  // reattach ownership is transfered.
  attached_tabstrip_->ReleaseDragController();
  if (release_capture == RELEASE_CAPTURE)
    attached_tabstrip_->GetWidget()->ReleaseCapture();

  mouse_move_direction_ = kMovedMouseLeft | kMovedMouseRight;

  std::vector<gfx::Rect> drag_bounds = CalculateBoundsForDraggedTabs();
  TabStripModel* attached_model = GetModel(attached_tabstrip_);
  std::vector<TabRendererData> tab_data;
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    tab_data.push_back(drag_data_[i].attached_tab->data());
    int index = attached_model->GetIndexOfWebContents(drag_data_[i].contents);
    DCHECK_NE(-1, index);

    // Hide the tab so that the user doesn't see it animate closed.
    drag_data_[i].attached_tab->SetVisible(false);
    drag_data_[i].attached_tab->set_detached();

    attached_model->DetachWebContentsAt(index);

    // Detaching may end up deleting the tab, drop references to it.
    drag_data_[i].attached_tab = NULL;
  }

  // If we've removed the last Tab from the TabStrip, hide the frame now.
  if (!attached_model->empty()) {
    if (!selection_model_before_attach_.empty() &&
        selection_model_before_attach_.active() >= 0 &&
        selection_model_before_attach_.active() < attached_model->count()) {
      // Restore the selection.
      attached_model->SetSelectionFromModel(selection_model_before_attach_);
    } else if (attached_tabstrip_ == source_tabstrip_ &&
               !initial_selection_model_.empty()) {
      RestoreInitialSelection();
    }
  }

  attached_tabstrip_->DraggedTabsDetached();
  attached_tabstrip_ = NULL;
}

void TabDragController::DetachIntoNewBrowserAndRunMoveLoop(
    const gfx::Point& point_in_screen) {
  if (GetModel(attached_tabstrip_)->count() ==
      static_cast<int>(drag_data_.size())) {
    // All the tabs in a browser are being dragged but all the tabs weren't
    // initially being dragged. For this to happen the user would have to
    // start dragging a set of tabs, the other tabs close, then detach.
    RunMoveLoop(GetWindowOffset(point_in_screen));
    return;
  }

  const int last_tabstrip_width = attached_tabstrip_->GetTabAreaWidth();
  std::vector<gfx::Rect> drag_bounds = CalculateBoundsForDraggedTabs();
  OffsetX(GetAttachedDragPoint(point_in_screen).x(), &drag_bounds);

  gfx::Vector2d drag_offset;
  Browser* browser = CreateBrowserForDrag(
      attached_tabstrip_, point_in_screen, &drag_offset, &drag_bounds);

  BrowserView* dragged_browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  views::Widget* dragged_widget = dragged_browser_view->GetWidget();

#if defined(USE_AURA)
  // Only Aura windows are gesture consumers.
  gfx::NativeView attached_native_view =
      attached_tabstrip_->GetWidget()->GetNativeView();
  ui::GestureRecognizer::Get()->TransferEventsTo(
      attached_native_view, dragged_widget->GetNativeView(),
      ui::GestureRecognizer::ShouldCancelTouches::DontCancel);
#endif

  Detach(can_release_capture_ ? RELEASE_CAPTURE : DONT_RELEASE_CAPTURE);

  dragged_widget->SetVisibilityChangedAnimationsEnabled(false);
  Attach(dragged_browser_view->tabstrip(), gfx::Point());
  AdjustBrowserAndTabBoundsForDrag(last_tabstrip_width,
                                   point_in_screen,
                                   &drag_bounds);
  browser->window()->Show();
  dragged_widget->SetVisibilityChangedAnimationsEnabled(true);
  // Activate may trigger a focus loss, destroying us.
  {
    base::WeakPtr<TabDragController> ref(weak_factory_.GetWeakPtr());
    browser->window()->Activate();
    if (!ref)
      return;
  }
  RunMoveLoop(drag_offset);
}

void TabDragController::RunMoveLoop(const gfx::Vector2d& drag_offset) {
  // If the user drags the whole window we'll assume they are going to attach to
  // another window and therefore want to reorder.
  move_behavior_ = REORDER;

  move_loop_widget_ = GetAttachedBrowserWidget();
  DCHECK(move_loop_widget_);
  move_loop_widget_->AddObserver(this);
  added_observer_to_move_loop_widget_ = true;
  is_dragging_window_ = true;
  base::WeakPtr<TabDragController> ref(weak_factory_.GetWeakPtr());
  if (can_release_capture_) {
    // Running the move loop releases mouse capture, which triggers destroying
    // the drag loop. Release mouse capture now while the DragController is not
    // owned by the TabStrip.
    attached_tabstrip_->ReleaseDragController();
    attached_tabstrip_->GetWidget()->ReleaseCapture();
    attached_tabstrip_->OwnDragController(this);
  }
  const views::Widget::MoveLoopSource move_loop_source =
      event_source_ == EVENT_SOURCE_MOUSE ?
      views::Widget::MOVE_LOOP_SOURCE_MOUSE :
      views::Widget::MOVE_LOOP_SOURCE_TOUCH;
  const views::Widget::MoveLoopEscapeBehavior escape_behavior =
      is_dragging_new_browser_ ?
          views::Widget::MOVE_LOOP_ESCAPE_BEHAVIOR_HIDE :
          views::Widget::MOVE_LOOP_ESCAPE_BEHAVIOR_DONT_HIDE;
  views::Widget::MoveLoopResult result =
      move_loop_widget_->RunMoveLoop(
          drag_offset, move_loop_source, escape_behavior);
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_DRAG_LOOP_DONE,
      content::NotificationService::AllBrowserContextsAndSources(),
      content::NotificationService::NoDetails());

  if (!ref)
    return;
  if (move_loop_widget_) {
    move_loop_widget_->RemoveObserver(this);
    move_loop_widget_ = nullptr;
  }
  is_dragging_window_ = false;
  waiting_for_run_loop_to_exit_ = false;
  if (end_run_loop_behavior_ == END_RUN_LOOP_CONTINUE_DRAGGING) {
    end_run_loop_behavior_ = END_RUN_LOOP_STOP_DRAGGING;
    if (tab_strip_to_attach_to_after_exit_) {
      gfx::Point point_in_screen(GetCursorScreenPoint());
      Detach(DONT_RELEASE_CAPTURE);
      Attach(tab_strip_to_attach_to_after_exit_, point_in_screen);
      // Move the tabs into position.
      MoveAttached(point_in_screen);
      attached_tabstrip_->GetWidget()->Activate();
      // Activate may trigger a focus loss, destroying us.
      if (!ref)
        return;
      tab_strip_to_attach_to_after_exit_ = NULL;
    }
    DCHECK(attached_tabstrip_);
    attached_tabstrip_->GetWidget()->SetCapture(attached_tabstrip_);
  } else if (active_) {
    EndDrag(result == views::Widget::MOVE_LOOP_CANCELED ?
            END_DRAG_CANCEL : END_DRAG_COMPLETE);
  }
}

int TabDragController::GetInsertionIndexFrom(const gfx::Rect& dragged_bounds,
                                             int start) const {
  const int last_tab = attached_tabstrip_->tab_count() - 1;
  // Make the actual "drag insertion point" be just after the leading edge of
  // the first dragged tab.  This is closer to where the user thinks of the tab
  // as "starting" than just dragged_bounds.x(), especially with narrow tabs.
  const int dragged_x = dragged_bounds.x() + kLeadingWidthForDrag;
  if (start < 0 || start > last_tab ||
      dragged_x < attached_tabstrip_->ideal_bounds(start).x())
    return -1;

  for (int i = start; i <= last_tab; ++i) {
    const gfx::Rect& ideal_bounds = attached_tabstrip_->ideal_bounds(i);
    if (dragged_x < (ideal_bounds.x() + (ideal_bounds.width() / 2)))
      return i;
  }

  return (dragged_x < attached_tabstrip_->ideal_bounds(last_tab).right()) ?
      (last_tab + 1) : -1;
}

int TabDragController::GetInsertionIndexFromReversed(
    const gfx::Rect& dragged_bounds,
    int start) const {
  // Make the actual "drag insertion point" be just after the leading edge of
  // the first dragged tab.  This is closer to where the user thinks of the tab
  // as "starting" than just dragged_bounds.x(), especially with narrow tabs.
  const int dragged_x = dragged_bounds.x() + kLeadingWidthForDrag;
  if (start < 0 || start >= attached_tabstrip_->tab_count() ||
      dragged_x >= attached_tabstrip_->ideal_bounds(start).right())
    return -1;

  for (int i = start; i >= 0; --i) {
    const gfx::Rect& ideal_bounds = attached_tabstrip_->ideal_bounds(i);
    if (dragged_x >= (ideal_bounds.x() + (ideal_bounds.width() / 2)))
      return i + 1;
  }

  return (dragged_x >= attached_tabstrip_->ideal_bounds(0).x()) ? 0 : -1;
}

int TabDragController::GetInsertionIndexForDraggedBounds(
    const gfx::Rect& dragged_bounds) const {
  // If the strip has no tabs, the only position to insert at is 0.
  const int tab_count = attached_tabstrip_->tab_count();
  if (!tab_count)
    return 0;

  int index = -1;
  if (attached_tabstrip_->touch_layout_.get()) {
    index = GetInsertionIndexForDraggedBoundsStacked(dragged_bounds);
    if (index != -1) {
      // Only move the tab to the left/right if the user actually moved the
      // mouse that way. This is necessary as tabs with stacked tabs
      // before/after them have multiple drag positions.
      int active_index = attached_tabstrip_->touch_layout_->active_index();
      if ((index < active_index &&
           (mouse_move_direction_ & kMovedMouseLeft) == 0) ||
          (index > active_index &&
           (mouse_move_direction_ & kMovedMouseRight) == 0)) {
        index = active_index;
      }
    }
  } else {
    index = GetInsertionIndexFrom(dragged_bounds, 0);
  }
  if (index == -1) {
    const int last_tab_right =
        attached_tabstrip_->ideal_bounds(tab_count - 1).right();
    index = (dragged_bounds.right() > last_tab_right) ? tab_count : 0;
  }

  const Tab* last_visible_tab = attached_tabstrip_->GetLastVisibleTab();
  int last_insertion_point = last_visible_tab ?
      (attached_tabstrip_->GetModelIndexOfTab(last_visible_tab) + 1) : 0;
  if (drag_data_[0].attached_tab) {
    // We're not in the process of attaching, so clamp the insertion point to
    // keep it within the visible region.
    last_insertion_point = std::max(
        0, last_insertion_point - static_cast<int>(drag_data_.size()));
  }

  // Ensure the first dragged tab always stays in the visible index range.
  return std::min(index, last_insertion_point);
}

bool TabDragController::ShouldDragToNextStackedTab(
    const gfx::Rect& dragged_bounds,
    int index) const {
  if (index + 1 >= attached_tabstrip_->tab_count() ||
      !attached_tabstrip_->touch_layout_->IsStacked(index + 1) ||
      (mouse_move_direction_ & kMovedMouseRight) == 0)
    return false;

  int active_x = attached_tabstrip_->ideal_bounds(index).x();
  int next_x = attached_tabstrip_->ideal_bounds(index + 1).x();
  int mid_x = std::min(next_x - kStackedDistance,
                       active_x + (next_x - active_x) / 4);
  // TODO(pkasting): Should this add kLeadingWidthForDrag as
  // GetInsertionIndexFrom() does?
  return dragged_bounds.x() >= mid_x;
}

bool TabDragController::ShouldDragToPreviousStackedTab(
    const gfx::Rect& dragged_bounds,
    int index) const {
  if (index - 1 < attached_tabstrip_->GetPinnedTabCount() ||
      !attached_tabstrip_->touch_layout_->IsStacked(index - 1) ||
      (mouse_move_direction_ & kMovedMouseLeft) == 0)
    return false;

  int active_x = attached_tabstrip_->ideal_bounds(index).x();
  int previous_x = attached_tabstrip_->ideal_bounds(index - 1).x();
  int mid_x = std::max(previous_x + kStackedDistance,
                       active_x - (active_x - previous_x) / 4);
  // TODO(pkasting): Should this add kLeadingWidthForDrag as
  // GetInsertionIndexFrom() does?
  return dragged_bounds.x() <= mid_x;
}

int TabDragController::GetInsertionIndexForDraggedBoundsStacked(
    const gfx::Rect& dragged_bounds) const {
  StackedTabStripLayout* touch_layout = attached_tabstrip_->touch_layout_.get();
  int active_index = touch_layout->active_index();
  // Search from the active index to the front of the tabstrip. Do this as tabs
  // overlap each other from the active index.
  int index = GetInsertionIndexFromReversed(dragged_bounds, active_index);
  if (index != active_index)
    return index;
  if (index == -1)
    return GetInsertionIndexFrom(dragged_bounds, active_index + 1);

  // The position to drag to corresponds to the active tab. If the next/previous
  // tab is stacked, then shorten the distance used to determine insertion
  // bounds. We do this as GetInsertionIndexFrom() uses the bounds of the
  // tabs. When tabs are stacked the next/previous tab is on top of the tab.
  if (active_index + 1 < attached_tabstrip_->tab_count() &&
      touch_layout->IsStacked(active_index + 1)) {
    index = GetInsertionIndexFrom(dragged_bounds, active_index + 1);
    if (index == -1 && ShouldDragToNextStackedTab(dragged_bounds, active_index))
      index = active_index + 1;
    else if (index == -1)
      index = active_index;
  } else if (ShouldDragToPreviousStackedTab(dragged_bounds, active_index)) {
    index = active_index - 1;
  }
  return index;
}

gfx::Rect TabDragController::GetDraggedViewTabStripBounds(
    const gfx::Point& tab_strip_point) {
  // attached_tab is NULL when inserting into a new tabstrip.
  if (source_tab_drag_data()->attached_tab) {
    return gfx::Rect(tab_strip_point.x(), tab_strip_point.y(),
                     source_tab_drag_data()->attached_tab->width(),
                     source_tab_drag_data()->attached_tab->height());
  }

  return gfx::Rect(tab_strip_point.x(), tab_strip_point.y(),
                   attached_tabstrip_->current_active_width(),
                   Tab::GetStandardSize().height());
}

gfx::Point TabDragController::GetAttachedDragPoint(
    const gfx::Point& point_in_screen) {
  DCHECK(attached_tabstrip_);  // The tab must be attached.

  gfx::Point tab_loc(point_in_screen);
  views::View::ConvertPointFromScreen(attached_tabstrip_, &tab_loc);
  const int x =
      attached_tabstrip_->GetMirroredXInView(tab_loc.x()) - mouse_offset_.x();

  // TODO: consider caching this.
  std::vector<Tab*> attached_tabs;
  for (size_t i = 0; i < drag_data_.size(); ++i)
    attached_tabs.push_back(drag_data_[i].attached_tab);
  const int size = attached_tabstrip_->GetSizeNeededForTabs(attached_tabs);
  const int max_x = attached_tabstrip_->width() - size;
  return gfx::Point(std::min(std::max(x, 0), max_x), 0);
}

std::vector<Tab*> TabDragController::GetTabsMatchingDraggedContents(
    TabStrip* tabstrip) {
  TabStripModel* model = GetModel(attached_tabstrip_);
  std::vector<Tab*> tabs;
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    int model_index = model->GetIndexOfWebContents(drag_data_[i].contents);
    if (model_index == TabStripModel::kNoTab)
      return std::vector<Tab*>();
    tabs.push_back(tabstrip->tab_at(model_index));
  }
  return tabs;
}

std::vector<gfx::Rect> TabDragController::CalculateBoundsForDraggedTabs() {
  std::vector<gfx::Rect> drag_bounds;
  std::vector<Tab*> attached_tabs;
  for (size_t i = 0; i < drag_data_.size(); ++i)
    attached_tabs.push_back(drag_data_[i].attached_tab);
  attached_tabstrip_->CalculateBoundsForDraggedTabs(attached_tabs,
                                                    &drag_bounds);
  return drag_bounds;
}

void TabDragController::EndDragImpl(EndDragType type) {
  DCHECK(active_);
  active_ = false;

  bring_to_front_timer_.Stop();
  move_stacked_timer_.Stop();

  if (is_dragging_window_) {
    waiting_for_run_loop_to_exit_ = true;

    // End the nested drag loop.
    GetAttachedBrowserWidget()->EndMoveLoop();
  }

  if (type != TAB_DESTROYED) {
    // We only finish up the drag if we were actually dragging. If start_drag_
    // is false, the user just clicked and released and didn't move the mouse
    // enough to trigger a drag.
    if (started_drag_) {
      RestoreFocus();
      if (type == CANCELED)
        RevertDrag();
      else
        CompleteDrag();
    }
  } else if (drag_data_.size() > 1) {
    initial_selection_model_.Clear();
    if (started_drag_)
      RevertDrag();
  }  // else case the only tab we were dragging was deleted. Nothing to do.

  // Clear out drag data so we don't attempt to do anything with it.
  drag_data_.clear();

  TabStrip* owning_tabstrip = attached_tabstrip_ ?
      attached_tabstrip_ : source_tabstrip_;
  owning_tabstrip->DestroyDragController();
}

void TabDragController::RevertDrag() {
  std::vector<Tab*> tabs;
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    if (drag_data_[i].contents) {
      // Contents is NULL if a tab was destroyed while the drag was under way.
      tabs.push_back(drag_data_[i].attached_tab);
      RevertDragAt(i);
    }
  }

  if (attached_tabstrip_) {
    if (did_restore_window_)
      MaximizeAttachedWindow();
    if (attached_tabstrip_ == source_tabstrip_) {
      source_tabstrip_->StoppedDraggingTabs(
          tabs, initial_tab_positions_, move_behavior_ == MOVE_VISIBLE_TABS,
          false);
    } else {
      attached_tabstrip_->DraggedTabsDetached();
    }
  }

  if (initial_selection_model_.empty())
    ResetSelection(GetModel(source_tabstrip_));
  else
    GetModel(source_tabstrip_)->SetSelectionFromModel(initial_selection_model_);

  if (source_tabstrip_)
    source_tabstrip_->GetWidget()->Activate();
}

void TabDragController::ResetSelection(TabStripModel* model) {
  DCHECK(model);
  ui::ListSelectionModel selection_model;
  bool has_one_valid_tab = false;
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    // |contents| is NULL if a tab was deleted out from under us.
    if (drag_data_[i].contents) {
      int index = model->GetIndexOfWebContents(drag_data_[i].contents);
      DCHECK_NE(-1, index);
      selection_model.AddIndexToSelection(index);
      if (!has_one_valid_tab || i == source_tab_index_) {
        // Reset the active/lead to the first tab. If the source tab is still
        // valid we'll reset these again later on.
        selection_model.set_active(index);
        selection_model.set_anchor(index);
        has_one_valid_tab = true;
      }
    }
  }
  if (!has_one_valid_tab)
    return;

  model->SetSelectionFromModel(selection_model);
}

void TabDragController::RestoreInitialSelection() {
  // First time detaching from the source tabstrip. Reset selection model to
  // initial_selection_model_. Before resetting though we have to remove all
  // the tabs from initial_selection_model_ as it was created with the tabs
  // still there.
  ui::ListSelectionModel selection_model;
  selection_model.Copy(initial_selection_model_);
  for (DragData::const_reverse_iterator i(drag_data_.rbegin());
       i != drag_data_.rend(); ++i) {
    selection_model.DecrementFrom(i->source_model_index);
  }
  // We may have cleared out the selection model. Only reset it if it
  // contains something.
  if (selection_model.empty())
    return;

  // The anchor/active may have been among the tabs that were dragged out. Force
  // the anchor/active to be valid.
  if (selection_model.anchor() == ui::ListSelectionModel::kUnselectedIndex)
    selection_model.set_anchor(selection_model.selected_indices()[0]);
  if (selection_model.active() == ui::ListSelectionModel::kUnselectedIndex)
    selection_model.set_active(selection_model.selected_indices()[0]);
  GetModel(source_tabstrip_)->SetSelectionFromModel(selection_model);
}

void TabDragController::RevertDragAt(size_t drag_index) {
  DCHECK(started_drag_);
  DCHECK(source_tabstrip_);

  base::AutoReset<bool> setter(&is_mutating_, true);
  TabDragData* data = &(drag_data_[drag_index]);
  if (attached_tabstrip_) {
    int index =
        GetModel(attached_tabstrip_)->GetIndexOfWebContents(data->contents);
    if (attached_tabstrip_ != source_tabstrip_) {
      // The Tab was inserted into another TabStrip. We need to put it back
      // into the original one.
      GetModel(attached_tabstrip_)->DetachWebContentsAt(index);
      // TODO(beng): (Cleanup) seems like we should use Attach() for this
      //             somehow.
      GetModel(source_tabstrip_)->InsertWebContentsAt(
          data->source_model_index, data->contents,
          (data->pinned ? TabStripModel::ADD_PINNED : 0));
    } else {
      // The Tab was moved within the TabStrip where the drag was initiated.
      // Move it back to the starting location.
      GetModel(source_tabstrip_)->MoveWebContentsAt(
          index, data->source_model_index, false);
    }
  } else {
    // The Tab was detached from the TabStrip where the drag began, and has not
    // been attached to any other TabStrip. We need to put it back into the
    // source TabStrip.
    GetModel(source_tabstrip_)->InsertWebContentsAt(
        data->source_model_index, data->contents,
        (data->pinned ? TabStripModel::ADD_PINNED : 0));
  }
}

void TabDragController::CompleteDrag() {
  DCHECK(started_drag_);

  if (attached_tabstrip_) {
    if (is_dragging_new_browser_ || did_restore_window_) {
      if (IsSnapped(attached_tabstrip_)) {
        was_source_maximized_ = false;
        was_source_fullscreen_ = false;
      }

      // If source window was maximized - maximize the new window as well.
      if (was_source_maximized_ || was_source_fullscreen_)
        MaximizeAttachedWindow();
    }
    attached_tabstrip_->StoppedDraggingTabs(
        GetTabsMatchingDraggedContents(attached_tabstrip_),
        initial_tab_positions_,
        move_behavior_ == MOVE_VISIBLE_TABS,
        true);
  } else {
    // Compel the model to construct a new window for the detached
    // WebContentses.
    views::Widget* widget = source_tabstrip_->GetWidget();
    gfx::Rect window_bounds(widget->GetRestoredBounds());
    window_bounds.set_origin(GetWindowCreatePoint(last_point_in_screen_));

    base::AutoReset<bool> setter(&is_mutating_, true);

    std::vector<TabStripModelDelegate::NewStripContents> contentses;
    for (size_t i = 0; i < drag_data_.size(); ++i) {
      TabStripModelDelegate::NewStripContents item;
      item.web_contents = drag_data_[i].contents;
      item.add_types = drag_data_[i].pinned ? TabStripModel::ADD_PINNED
                                            : TabStripModel::ADD_NONE;
      contentses.push_back(item);
    }

    Browser* new_browser =
        GetModel(source_tabstrip_)->delegate()->CreateNewStripWithContents(
            contentses, window_bounds, widget->IsMaximized());
    ResetSelection(new_browser->tab_strip_model());
    new_browser->window()->Show();
  }
}

void TabDragController::MaximizeAttachedWindow() {
  if (move_loop_widget_ && added_observer_to_move_loop_widget_) {
    // This function is only called when the drag is ending. At this point we
    // don't care about any subsequent moves to the widget, so we remove the
    // observer. If we didn't do this we could get told the widget moved and
    // attempt to do the wrong thing.
    move_loop_widget_->RemoveObserver(this);
    added_observer_to_move_loop_widget_ = false;
  }
  GetAttachedBrowserWidget()->Maximize();
#if defined(OS_CHROMEOS)
  if (was_source_fullscreen_) {
    // In fullscreen mode it is only possible to get here if the source
    // was in "immersive fullscreen" mode, so toggle it back on.
    GetAttachedBrowserWidget()->SetFullscreen(true);
  }
#endif
}

gfx::Rect TabDragController::GetViewScreenBounds(
    views::View* view) const {
  gfx::Point view_topleft;
  views::View::ConvertPointToScreen(view, &view_topleft);
  gfx::Rect view_screen_bounds = view->GetLocalBounds();
  view_screen_bounds.Offset(view_topleft.x(), view_topleft.y());
  return view_screen_bounds;
}

void TabDragController::BringWindowUnderPointToFront(
    const gfx::Point& point_in_screen) {
  gfx::NativeWindow window;
  if (GetLocalProcessWindow(point_in_screen, true, &window) ==
      Liveness::DELETED) {
    return;
  }

  // Only bring browser windows to front - only windows with a TabStrip can
  // be tab drag targets.
  if (!GetTabStripForWindow(window))
    return;

  if (window) {
    views::Widget* widget_window = views::Widget::GetWidgetForNativeWindow(
        window);
    if (!widget_window)
      return;

#if defined(OS_CHROMEOS)
    // TODO(varkha): The code below ensures that the phantom drag widget
    // is shown on top of browser windows. The code should be moved to ash/
    // and the phantom should be able to assert its top-most state on its own.
    // One strategy would be for DragWindowController to
    // be able to observe stacking changes to the phantom drag widget's
    // siblings in order to keep it on top. One way is to implement a
    // notification that is sent to a window parent's observers when a
    // stacking order is changed among the children of that same parent.
    // Note that OnWindowStackingChanged is sent only to the child that is the
    // argument of one of the Window::StackChildX calls and not to all its
    // siblings affected by the stacking change.
    aura::Window* browser_window = widget_window->GetNativeView();
    // Find a topmost non-popup window and stack the recipient browser above
    // it in order to avoid stacking the browser window on top of the phantom
    // drag widget created by DragWindowController in a second display.
    for (aura::Window::Windows::const_reverse_iterator it =
             browser_window->parent()->children().rbegin();
         it != browser_window->parent()->children().rend(); ++it) {
      // If the iteration reached the recipient browser window then it is
      // already topmost and it is safe to return with no stacking change.
      if (*it == browser_window)
        return;
      if ((*it)->type() != aura::client::WINDOW_TYPE_POPUP) {
        widget_window->StackAbove(*it);
        break;
      }
    }
#else
    widget_window->StackAtTop();
#endif

    // The previous call made the window appear on top of the dragged window,
    // move the dragged window to the front.
    if (is_dragging_window_)
      attached_tabstrip_->GetWidget()->StackAtTop();
  }
}

TabStripModel* TabDragController::GetModel(
    TabStrip* tabstrip) const {
  return static_cast<BrowserTabStripController*>(tabstrip->controller())->
      model();
}

views::Widget* TabDragController::GetAttachedBrowserWidget() {
  return attached_tabstrip_->GetWidget();
}

bool TabDragController::AreTabsConsecutive() {
  for (size_t i = 1; i < drag_data_.size(); ++i) {
    if (drag_data_[i - 1].source_model_index + 1 !=
        drag_data_[i].source_model_index) {
      return false;
    }
  }
  return true;
}

gfx::Rect TabDragController::CalculateDraggedBrowserBounds(
    TabStrip* source,
    const gfx::Point& point_in_screen,
    std::vector<gfx::Rect>* drag_bounds) {
  gfx::Point center(0, source->height() / 2);
  views::View::ConvertPointToWidget(source, &center);
  gfx::Rect new_bounds(source->GetWidget()->GetRestoredBounds());

  gfx::Rect work_area = display::Screen::GetScreen()
                            ->GetDisplayNearestPoint(last_point_in_screen_)
                            .work_area();
  if (new_bounds.size().width() >= work_area.size().width() &&
      new_bounds.size().height() >= work_area.size().height()) {
    new_bounds = work_area;
    new_bounds.Inset(kMaximizedWindowInset, kMaximizedWindowInset,
                     kMaximizedWindowInset, kMaximizedWindowInset);
    // Behave as if the |source| was maximized at the start of a drag since this
    // is consistent with a browser window creation logic in case of windows
    // that are as large as the |work_area|.
    was_source_maximized_ = true;
  }

  if (source->GetWidget()->IsMaximized()) {
    // If the restore bounds is really small, we don't want to honor it
    // (dragging a really small window looks wrong), instead make sure the new
    // window is at least 50% the size of the old.
    const gfx::Size max_size(
        source->GetWidget()->GetWindowBoundsInScreen().size());
    new_bounds.set_width(
        std::max(max_size.width() / 2, new_bounds.width()));
    new_bounds.set_height(
        std::max(max_size.height() / 2, new_bounds.height()));
  }
  new_bounds.set_y(point_in_screen.y() - center.y());
  switch (GetDetachPosition(point_in_screen)) {
    case DETACH_BEFORE:
      new_bounds.set_x(point_in_screen.x() - center.x());
      new_bounds.Offset(-mouse_offset_.x(), 0);
      break;
    case DETACH_AFTER: {
      gfx::Point right_edge(source->width(), 0);
      views::View::ConvertPointToWidget(source, &right_edge);
      new_bounds.set_x(point_in_screen.x() - right_edge.x());
      new_bounds.Offset(drag_bounds->back().right() - mouse_offset_.x(), 0);
      OffsetX(-(*drag_bounds)[0].x(), drag_bounds);
      break;
    }
    default:
      break; // Nothing to do for DETACH_ABOVE_OR_BELOW.
  }

  // Account for the extra space above the tabstrip on restored windows versus
  // maximized windows.
  if (source->GetWidget()->IsMaximized()) {
    const auto* frame_view = static_cast<BrowserNonClientFrameView*>(
        source->GetWidget()->non_client_view()->frame_view());
    new_bounds.Offset(
        0, frame_view->GetTopInset(false) - frame_view->GetTopInset(true));
  }
  return new_bounds;
}

void TabDragController::AdjustBrowserAndTabBoundsForDrag(
    int last_tabstrip_width,
    const gfx::Point& point_in_screen,
    std::vector<gfx::Rect>* drag_bounds) {
  attached_tabstrip_->InvalidateLayout();
  attached_tabstrip_->DoLayout();
  const int dragged_tabstrip_width = attached_tabstrip_->GetTabAreaWidth();

  // If the new tabstrip is smaller than the old resize the tabs.
  if (dragged_tabstrip_width < last_tabstrip_width) {
    const float leading_ratio =
        drag_bounds->front().x() / static_cast<float>(last_tabstrip_width);
    *drag_bounds = CalculateBoundsForDraggedTabs();

    if (drag_bounds->back().right() < dragged_tabstrip_width) {
      const int delta_x =
          std::min(static_cast<int>(leading_ratio * dragged_tabstrip_width),
                   dragged_tabstrip_width -
                       (drag_bounds->back().right() -
                        drag_bounds->front().x()));
      OffsetX(delta_x, drag_bounds);
    }

    // Reposition the restored window such that the tab that was dragged remains
    // under the mouse cursor.
    gfx::Point offset(
        static_cast<int>((*drag_bounds)[source_tab_index_].width() *
                         offset_to_width_ratio_) +
        (*drag_bounds)[source_tab_index_].x(), 0);
    views::View::ConvertPointToWidget(attached_tabstrip_, &offset);
    gfx::Rect bounds = GetAttachedBrowserWidget()->GetWindowBoundsInScreen();
    bounds.set_x(point_in_screen.x() - offset.x());
    GetAttachedBrowserWidget()->SetBounds(bounds);
  }
  attached_tabstrip_->SetTabBoundsForDrag(*drag_bounds);
}

Browser* TabDragController::CreateBrowserForDrag(
    TabStrip* source,
    const gfx::Point& point_in_screen,
    gfx::Vector2d* drag_offset,
    std::vector<gfx::Rect>* drag_bounds) {
  gfx::Rect new_bounds(CalculateDraggedBrowserBounds(source,
                                                     point_in_screen,
                                                     drag_bounds));
  *drag_offset = point_in_screen - new_bounds.origin();

  Profile* profile =
      Profile::FromBrowserContext(drag_data_[0].contents->GetBrowserContext());
  Browser::CreateParams create_params(Browser::TYPE_TABBED, profile, true);
  create_params.initial_bounds = new_bounds;
  Browser* browser = new Browser(create_params);
  is_dragging_new_browser_ = true;
  // If the window is created maximized then the bounds we supplied are ignored.
  // We need to reset them again so they are honored.
  browser->window()->SetBounds(new_bounds);

  return browser;
}

gfx::Point TabDragController::GetCursorScreenPoint() {
#if defined(OS_CHROMEOS)
  // TODO(erg): Temporarily disable getting location from the gesture
  // recognizer in mash until the mus side/window manager side RunMoveLoop() is
  // fixed to understand routing touch events. crbug.com/769507
  if (!ash_util::IsRunningInMash() && event_source_ == EVENT_SOURCE_TOUCH &&
      aura::Env::GetInstance()->is_touch_down()) {
    views::Widget* widget = GetAttachedBrowserWidget();
    DCHECK(widget);
    aura::Window* widget_window = widget->GetNativeWindow();
    DCHECK(widget_window->GetRootWindow());
    gfx::PointF touch_point_f;
    bool got_touch_point = ui::GestureRecognizer::Get()->
        GetLastTouchPointForTarget(widget_window, &touch_point_f);
    CHECK(got_touch_point);
    gfx::Point touch_point = gfx::ToFlooredPoint(touch_point_f);
    wm::ConvertPointToScreen(widget_window->GetRootWindow(), &touch_point);
    return touch_point;
  }
#endif

  return display::Screen::GetScreen()->GetCursorScreenPoint();
}

gfx::Vector2d TabDragController::GetWindowOffset(
    const gfx::Point& point_in_screen) {
  TabStrip* owning_tabstrip = attached_tabstrip_ ?
      attached_tabstrip_ : source_tabstrip_;
  views::View* toplevel_view = owning_tabstrip->GetWidget()->GetContentsView();

  gfx::Point point = point_in_screen;
  views::View::ConvertPointFromScreen(toplevel_view, &point);
  return point.OffsetFromOrigin();
}

TabDragController::Liveness TabDragController::GetLocalProcessWindow(
    const gfx::Point& screen_point,
    bool exclude_dragged_view,
    gfx::NativeWindow* window) {
  std::set<gfx::NativeWindow> exclude;
  if (exclude_dragged_view) {
    gfx::NativeWindow dragged_window =
        attached_tabstrip_->GetWidget()->GetNativeWindow();
    if (dragged_window)
      exclude.insert(dragged_window);
  }
#if defined(OS_LINUX) && !defined(OS_CHROMEOS)
  // Exclude windows which are pending deletion via Browser::TabStripEmpty().
  // These windows can be returned in the Linux Aura port because the browser
  // window which was used for dragging is not hidden once all of its tabs are
  // attached to another browser window in DragBrowserToNewTabStrip().
  // TODO(pkotwicz): Fix this properly (crbug.com/358482)
  for (auto* browser : *BrowserList::GetInstance()) {
    if (browser->tab_strip_model()->empty())
      exclude.insert(browser->window()->GetNativeWindow());
  }
#endif
  base::WeakPtr<TabDragController> ref(weak_factory_.GetWeakPtr());
  *window = window_finder_->GetLocalProcessWindowAtPoint(screen_point, exclude);
  return ref ? Liveness::ALIVE : Liveness::DELETED;
}
