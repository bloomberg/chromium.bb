// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/tab_drag_controller2.h"

#include <math.h>
#include <set>

#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/app_modal_dialogs/message_box_handler.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/base_tab.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/dragged_tab_view.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_service.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/user_metrics.h"
#include "content/public/browser/web_contents.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/animation/animation.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/events.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/screen.h"
#include "ui/views/events/event.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

using content::UserMetricsAction;
using content::WebContents;

static const int kHorizontalMoveThreshold = 16;  // Pixels.

// If non-null there is a drag underway.
static TabDragController2* instance_;

namespace {

// Delay, in ms, during dragging before we bring a window to front.
const int kBringToFrontDelay = 750;

// Radius of the rect drawn by DockView.
const int kRoundedRectRadius = 4;

// Spacing between tab icons when DockView is showing a docking location that
// contains more than one tab.
const int kTabSpacing = 4;

// DockView is the view responsible for giving a visual indicator of where a
// dock is going to occur.

class DockView : public views::View {
 public:
  explicit DockView(DockInfo::Type type) : type_(type) {}

  virtual gfx::Size GetPreferredSize() {
    return gfx::Size(DockInfo::popup_width(), DockInfo::popup_height());
  }

  virtual void OnPaintBackground(gfx::Canvas* canvas) {
    SkRect outer_rect = { SkIntToScalar(0), SkIntToScalar(0),
                          SkIntToScalar(width()),
                          SkIntToScalar(height()) };

    // Fill the background rect.
    SkPaint paint;
    paint.setColor(SkColorSetRGB(108, 108, 108));
    paint.setStyle(SkPaint::kFill_Style);
    canvas->GetSkCanvas()->drawRoundRect(
        outer_rect, SkIntToScalar(kRoundedRectRadius),
        SkIntToScalar(kRoundedRectRadius), paint);

    ResourceBundle& rb = ResourceBundle::GetSharedInstance();

    SkBitmap* high_icon = rb.GetBitmapNamed(IDR_DOCK_HIGH);
    SkBitmap* wide_icon = rb.GetBitmapNamed(IDR_DOCK_WIDE);

    canvas->Save();
    bool rtl_ui = base::i18n::IsRTL();
    if (rtl_ui) {
      // Flip canvas to draw the mirrored tab images for RTL UI.
      canvas->Translate(gfx::Point(width(), 0));
      canvas->Scale(-1, 1);
    }
    int x_of_active_tab = width() / 2 + kTabSpacing / 2;
    int x_of_inactive_tab = width() / 2 - high_icon->width() - kTabSpacing / 2;
    switch (type_) {
      case DockInfo::LEFT_OF_WINDOW:
      case DockInfo::LEFT_HALF:
        if (!rtl_ui)
          std::swap(x_of_active_tab, x_of_inactive_tab);
        canvas->DrawBitmapInt(*high_icon, x_of_active_tab,
                              (height() - high_icon->height()) / 2);
        if (type_ == DockInfo::LEFT_OF_WINDOW) {
          DrawBitmapWithAlpha(canvas, *high_icon, x_of_inactive_tab,
                              (height() - high_icon->height()) / 2);
        }
        break;


      case DockInfo::RIGHT_OF_WINDOW:
      case DockInfo::RIGHT_HALF:
        if (rtl_ui)
          std::swap(x_of_active_tab, x_of_inactive_tab);
        canvas->DrawBitmapInt(*high_icon, x_of_active_tab,
                              (height() - high_icon->height()) / 2);
        if (type_ == DockInfo::RIGHT_OF_WINDOW) {
         DrawBitmapWithAlpha(canvas, *high_icon, x_of_inactive_tab,
                             (height() - high_icon->height()) / 2);
        }
        break;

      case DockInfo::TOP_OF_WINDOW:
        canvas->DrawBitmapInt(*wide_icon, (width() - wide_icon->width()) / 2,
                              height() / 2 - high_icon->height());
        break;

      case DockInfo::MAXIMIZE: {
        SkBitmap* max_icon = rb.GetBitmapNamed(IDR_DOCK_MAX);
        canvas->DrawBitmapInt(*max_icon, (width() - max_icon->width()) / 2,
                              (height() - max_icon->height()) / 2);
        break;
      }

      case DockInfo::BOTTOM_HALF:
      case DockInfo::BOTTOM_OF_WINDOW:
        canvas->DrawBitmapInt(*wide_icon, (width() - wide_icon->width()) / 2,
                              height() / 2 + kTabSpacing / 2);
        if (type_ == DockInfo::BOTTOM_OF_WINDOW) {
          DrawBitmapWithAlpha(canvas, *wide_icon,
              (width() - wide_icon->width()) / 2,
              height() / 2 - kTabSpacing / 2 - wide_icon->height());
        }
        break;

      default:
        NOTREACHED();
        break;
    }
    canvas->Restore();
  }

 private:
  void DrawBitmapWithAlpha(gfx::Canvas* canvas, const SkBitmap& image,
                           int x, int y) {
    SkPaint paint;
    paint.setAlpha(128);
    canvas->DrawBitmapInt(image, x, y, paint);
  }

  DockInfo::Type type_;

  DISALLOW_COPY_AND_ASSIGN(DockView);
};

// Returns the the x-coordinate of |point| if the type of tabstrip is horizontal
// otherwise returns the y-coordinate.
int MajorAxisValue(const gfx::Point& point, TabStrip* tabstrip) {
  return point.x();
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// DockDisplayer

// DockDisplayer is responsible for giving the user a visual indication of a
// possible dock position (as represented by DockInfo). DockDisplayer shows
// a window with a DockView in it. Two animations are used that correspond to
// the state of DockInfo::in_enable_area.
class TabDragController2::DockDisplayer : public ui::AnimationDelegate {
 public:
  DockDisplayer(TabDragController2* controller,
                const DockInfo& info)
      : controller_(controller),
        popup_(NULL),
        popup_view_(NULL),
        ALLOW_THIS_IN_INITIALIZER_LIST(animation_(this)),
        hidden_(false),
        in_enable_area_(info.in_enable_area()) {
    popup_ = new views::Widget;
    views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
    params.transparent = true;
    params.keep_on_top = true;
    params.bounds = info.GetPopupRect();
    popup_->Init(params);
    popup_->SetContentsView(new DockView(info.type()));
    popup_->SetOpacity(0x00);
    if (info.in_enable_area())
      animation_.Reset(1);
    else
      animation_.Show();
    popup_->Show();
    popup_view_ = popup_->GetNativeView();
  }

  ~DockDisplayer() {
    if (controller_)
      controller_->DockDisplayerDestroyed(this);
  }

  // Updates the state based on |in_enable_area|.
  void UpdateInEnabledArea(bool in_enable_area) {
    if (in_enable_area != in_enable_area_) {
      in_enable_area_ = in_enable_area;
      UpdateLayeredAlpha();
    }
  }

  // Resets the reference to the hosting TabDragController2. This is invoked
  // when the TabDragController2 is destoryed.
  void clear_controller() { controller_ = NULL; }

  // NativeView of the window we create.
  gfx::NativeView popup_view() { return popup_view_; }

  // Starts the hide animation. When the window is closed the
  // TabDragController2 is notified by way of the DockDisplayerDestroyed
  // method
  void Hide() {
    if (hidden_)
      return;

    if (!popup_) {
      delete this;
      return;
    }
    hidden_ = true;
    animation_.Hide();
  }

  virtual void AnimationProgressed(const ui::Animation* animation) {
    UpdateLayeredAlpha();
  }

  virtual void AnimationEnded(const ui::Animation* animation) {
    if (!hidden_)
      return;
    popup_->Close();
    delete this;
  }

  virtual void UpdateLayeredAlpha() {
    double scale = in_enable_area_ ? 1 : .5;
    popup_->SetOpacity(static_cast<unsigned char>(animation_.GetCurrentValue() *
        scale * 255.0));
    popup_->GetRootView()->SchedulePaint();
  }

 private:
  // TabDragController2 that created us.
  TabDragController2* controller_;

  // Window we're showing.
  views::Widget* popup_;

  // NativeView of |popup_|. We cache this to avoid the possibility of
  // invoking a method on popup_ after we close it.
  gfx::NativeView popup_view_;

  // Animation for when first made visible.
  ui::SlideAnimation animation_;

  // Have we been hidden?
  bool hidden_;

  // Value of DockInfo::in_enable_area.
  bool in_enable_area_;
};

TabDragController2::TabDragData::TabDragData()
    : contents(NULL),
      source_model_index(-1),
      attached_tab(NULL),
      pinned(false) {
}

TabDragController2::TabDragData::~TabDragData() {
}

///////////////////////////////////////////////////////////////////////////////
// TabDragController2, public:

TabDragController2::TabDragController2()
    : source_tabstrip_(NULL),
      attached_tabstrip_(NULL),
      is_dragging_window_(false),
      source_tab_offset_(0),
      offset_to_width_ratio_(0),
      old_focused_view_(NULL),
      last_move_screen_loc_(0),
      started_drag_(false),
      active_(true),
      source_tab_index_(std::numeric_limits<size_t>::max()),
      initial_move_(true),
      end_run_loop_behavior_(END_RUN_LOOP_STOP_DRAGGING),
      waiting_for_run_loop_to_exit_(false),
      tab_strip_to_attach_to_after_exit_(NULL),
      move_loop_browser_view_(NULL),
      destroyed_(NULL) {
  instance_ = this;
}

TabDragController2::~TabDragController2() {
  if (instance_ == this)
    instance_ = NULL;

  if (destroyed_)
    *destroyed_ = true;

  if (move_loop_browser_view_)
    move_loop_browser_view_->set_move_observer(NULL);

  if (source_tabstrip_)
    GetModel(source_tabstrip_)->RemoveObserver(this);

  MessageLoopForUI::current()->RemoveObserver(this);
}

void TabDragController2::Init(
    TabStrip* source_tabstrip,
    BaseTab* source_tab,
    const std::vector<BaseTab*>& tabs,
    const gfx::Point& mouse_offset,
    int source_tab_offset,
    const TabStripSelectionModel& initial_selection_model) {
  DCHECK(!tabs.empty());
  DCHECK(std::find(tabs.begin(), tabs.end(), source_tab) != tabs.end());
  source_tabstrip_ = source_tabstrip;
  GetModel(source_tabstrip_)->AddObserver(this);
  source_tab_offset_ = source_tab_offset;
  start_screen_point_ = GetCursorScreenPoint();
  mouse_offset_ = mouse_offset;

  drag_data_.resize(tabs.size());
  for (size_t i = 0; i < tabs.size(); ++i)
    InitTabDragData(tabs[i], &(drag_data_[i]));
  source_tab_index_ =
      std::find(tabs.begin(), tabs.end(), source_tab) - tabs.begin();

  // Listen for Esc key presses.
  MessageLoopForUI::current()->AddObserver(this);

  if (source_tab->width() > 0) {
    offset_to_width_ratio_ = static_cast<float>(source_tab_offset_) /
        static_cast<float>(source_tab->width());
  }
  InitWindowCreatePoint(source_tabstrip);
  initial_selection_model_.Copy(initial_selection_model);
}

// static
bool TabDragController2::IsActiveAndAttachedTo(TabStrip* tab_strip) {
  return instance_ && instance_->active_ &&
      instance_->attached_tabstrip_ == tab_strip;
}

// static
bool TabDragController2::IsActive() {
  return instance_ && instance_->active_;
}

void TabDragController2::InitTabDragData(BaseTab* tab,
                                         TabDragData* drag_data) {
  drag_data->source_model_index =
      source_tabstrip_->GetModelIndexOfBaseTab(tab);
  drag_data->contents = GetModel(source_tabstrip_)->GetTabContentsAt(
      drag_data->source_model_index);
  drag_data->pinned = source_tabstrip_->IsTabPinned(tab);
  registrar_.Add(
      this,
      content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
      content::Source<WebContents>(drag_data->contents->web_contents()));
}

void TabDragController2::Drag() {
  bring_to_front_timer_.Stop();

  if (waiting_for_run_loop_to_exit_)
    return;

  if (!started_drag_) {
    if (!CanStartDrag())
      return;  // User hasn't dragged far enough yet.

    started_drag_ = true;
    SaveFocus();
    Attach(source_tabstrip_, gfx::Point());
    if (static_cast<int>(drag_data_.size()) ==
        GetModel(source_tabstrip_)->count()) {
      RunMoveLoop();  // Runs a nested loop, returning when done.
      return;
    }
  }

  ContinueDragging();
}

void TabDragController2::EndDrag(bool canceled) {
  // We can't revert if the source tabstrip was destroyed during the drag.
  EndDragImpl(canceled && source_tabstrip_ ? CANCELED : NORMAL);
}

bool TabDragController2::GetStartedDrag() const {
  return started_drag_;
}

///////////////////////////////////////////////////////////////////////////////
// TabDragController2, content::NotificationObserver implementation:

void TabDragController2::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, content::NOTIFICATION_WEB_CONTENTS_DESTROYED);
  WebContents* destroyed_contents = content::Source<WebContents>(source).ptr();
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    if (drag_data_[i].contents->web_contents() == destroyed_contents) {
      drag_data_[i].contents = NULL;
      EndDragImpl(TAB_DESTROYED);
      return;
    }
  }
  // If we get here it means we got notification for a tab we don't know about.
  NOTREACHED();
}

///////////////////////////////////////////////////////////////////////////////
// TabDragController2, MessageLoop::Observer implementation:

#if defined(OS_WIN) || defined(USE_AURA)
base::EventStatus TabDragController2::WillProcessEvent(
    const base::NativeEvent& event) {
  return base::EVENT_CONTINUE;
}

void TabDragController2::DidProcessEvent(const base::NativeEvent& event) {
  // If the user presses ESC during a drag, we need to abort and revert things
  // to the way they were. This is the most reliable way to do this since no
  // single view or window reliably receives events throughout all the various
  // kinds of tab dragging.
  if (ui::EventTypeFromNative(event) == ui::ET_KEY_PRESSED &&
      ui::KeyboardCodeFromNative(event) == ui::VKEY_ESCAPE) {
    EndDrag(true);
  }
}
#endif

void TabDragController2::OnWidgetMoved() {
  Drag();
}

void TabDragController2::TabStripEmpty() {
  GetModel(source_tabstrip_)->RemoveObserver(this);
  // NULL out source_tabstrip_ so that we don't attempt to add back to in (in
  // the case of a revert).
  source_tabstrip_ = NULL;
}

///////////////////////////////////////////////////////////////////////////////
// TabDragController2, private:

void TabDragController2::InitWindowCreatePoint(TabStrip* tab_strip) {
  // window_create_point_ is only used in CompleteDrag() (through
  // GetWindowCreatePoint() to get the start point of the docked window) when
  // the attached_tabstrip_ is NULL and all the window's related bound
  // information are obtained from tab_strip. So, we need to get the
  // first_tab based on tab_strip, not attached_tabstrip_. Otherwise,
  // the window_create_point_ is not in the correct coordinate system. Please
  // refer to http://crbug.com/6223 comment #15 for detailed information.
  views::View* first_tab = tab_strip->base_tab_at_tab_index(0);
  views::View::ConvertPointToWidget(first_tab, &first_source_tab_point_);
  window_create_point_ = first_source_tab_point_;
  window_create_point_.Offset(mouse_offset_.x(), mouse_offset_.y());
}

gfx::Point TabDragController2::GetWindowCreatePoint() const {
  gfx::Point cursor_point = GetCursorScreenPoint();
  if (dock_info_.type() != DockInfo::NONE && dock_info_.in_enable_area()) {
    // If we're going to dock, we need to return the exact coordinate,
    // otherwise we may attempt to maximize on the wrong monitor.
    return cursor_point;
  }
  // If the cursor is outside the monitor area, move it inside. For example,
  // dropping a tab onto the task bar on Windows produces this situation.
  gfx::Rect work_area = gfx::Screen::GetMonitorWorkAreaNearestPoint(
      cursor_point);
  if (!work_area.IsEmpty()) {
    if (cursor_point.x() < work_area.x())
      cursor_point.set_x(work_area.x());
    else if (cursor_point.x() > work_area.right())
      cursor_point.set_x(work_area.right());
    if (cursor_point.y() < work_area.y())
      cursor_point.set_y(work_area.y());
    else if (cursor_point.y() > work_area.bottom())
      cursor_point.set_y(work_area.bottom());
  }
  return gfx::Point(cursor_point.x() - window_create_point_.x(),
                    cursor_point.y() - window_create_point_.y());
}

void TabDragController2::UpdateDockInfo(const gfx::Point& screen_point) {
  // Update the DockInfo for the current mouse coordinates.
  DockInfo dock_info = GetDockInfoAtPoint(screen_point);
  if (!dock_info.equals(dock_info_)) {
    // DockInfo for current position differs.
    if (dock_info_.type() != DockInfo::NONE &&
        !dock_controllers_.empty()) {
      // Hide old visual indicator.
      dock_controllers_.back()->Hide();
    }
    dock_info_ = dock_info;
    if (dock_info_.type() != DockInfo::NONE) {
      // Show new docking position.
      DockDisplayer* controller = new DockDisplayer(this, dock_info_);
      if (controller->popup_view()) {
        dock_controllers_.push_back(controller);
        dock_windows_.insert(controller->popup_view());
      } else {
        delete controller;
      }
    }
  } else if (dock_info_.type() != DockInfo::NONE &&
             !dock_controllers_.empty()) {
    // Current dock position is the same as last, update the controller's
    // in_enable_area state as it may have changed.
    dock_controllers_.back()->UpdateInEnabledArea(dock_info_.in_enable_area());
  }
}

void TabDragController2::SaveFocus() {
  DCHECK(!old_focused_view_);  // This should only be invoked once.
  DCHECK(source_tabstrip_);
  old_focused_view_ = source_tabstrip_->GetFocusManager()->GetFocusedView();
  source_tabstrip_->GetFocusManager()->SetFocusedView(source_tabstrip_);
}

void TabDragController2::RestoreFocus() {
  if (old_focused_view_ && attached_tabstrip_ == source_tabstrip_)
    old_focused_view_->GetFocusManager()->SetFocusedView(old_focused_view_);
  old_focused_view_ = NULL;
}

bool TabDragController2::CanStartDrag() const {
  // Determine if the mouse has moved beyond a minimum elasticity distance in
  // any direction from the starting point.
  static const int kMinimumDragDistance = 10;
  gfx::Point screen_point = GetCursorScreenPoint();
  int x_offset = abs(screen_point.x() - start_screen_point_.x());
  int y_offset = abs(screen_point.y() - start_screen_point_.y());
  return sqrt(pow(static_cast<float>(x_offset), 2) +
              pow(static_cast<float>(y_offset), 2)) > kMinimumDragDistance;
}

void TabDragController2::ContinueDragging() {
  DCHECK(attached_tabstrip_);

  // Note that the coordinates given to us by |drag_event| are basically
  // useless, since they're in source_tab_ coordinates. On the surface, you'd
  // think we could just convert them to screen coordinates, however in the
  // situation where we're dragging the last tab in a window when multiple
  // windows are open, the coordinates of |source_tab_| are way off in
  // hyperspace since the window was moved there instead of being closed so
  // that we'd keep receiving events. And our ConvertPointToScreen methods
  // aren't really multi-screen aware. So really it's just safer to get the
  // actual position of the mouse cursor directly from Windows here, which is
  // guaranteed to be correct regardless of monitor config.
  gfx::Point screen_point = GetCursorScreenPoint();

  // Determine whether or not we have dragged over a compatible TabStrip in
  // another browser window. If we have, we should attach to it and start
  // dragging within it.
  TabStrip* target_tabstrip = GetTabStripForPoint(screen_point);
  bool tab_strip_changed = (target_tabstrip != attached_tabstrip_);
  if (tab_strip_changed) {
    if (!target_tabstrip) {
      DetachIntoNewBrowserAndRunMoveLoop(screen_point);
      return;
    }
    if (is_dragging_window_) {
      BrowserView* browser_view = GetAttachedBrowserView();
      // Need to release the drag controller before starting the move loop as
      // it's going to trigger capture lost, which cancels drag.
      attached_tabstrip_->ReleaseDragController();
      target_tabstrip->OwnDragController(this);
      // Disable animations so that we don't see a close animation on aero.
      browser_view->GetWidget()->SetVisibilityChangedAnimationsEnabled(false);
      browser_view->GetWidget()->ReleaseMouseCapture();
      // EndMoveLoop is going to snap the window back to it's original location.
      // Hide it so users don't see this.
      browser_view->GetWidget()->Hide();
      browser_view->GetWidget()->EndMoveLoop();
      tab_strip_to_attach_to_after_exit_ = target_tabstrip;
      waiting_for_run_loop_to_exit_ = true;
      end_run_loop_behavior_ = END_RUN_LOOP_CONTINUE_DRAGGING;
      // Ideally we would swap the tabs now, but on windows it seems that
      // running the move loop implicitly activates the window when done,
      // leading to all sorts of flicker. So, instead we process the move after
      // the loop completes.
      return;
    }
    Detach();
    Attach(target_tabstrip, screen_point);
  }
  if (is_dragging_window_) {
    bring_to_front_timer_.Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(kBringToFrontDelay), this,
        &TabDragController2::BringWindowUnderMouseToFront);
  }

  UpdateDockInfo(screen_point);

  if (!is_dragging_window_) {
    MoveAttached(screen_point);
    if (tab_strip_changed) {
      // Move the corresponding window to the front. We do this after the move
      // as on windows activate triggers a synchronous paint.
      attached_tabstrip_->GetWidget()->Activate();
    }
  }
}

void TabDragController2::MoveAttached(const gfx::Point& screen_point) {
  DCHECK(attached_tabstrip_);
  DCHECK(!is_dragging_window_);

  gfx::Point dragged_view_point = GetAttachedDragPoint(screen_point);

  TabStrip* tab_strip = static_cast<TabStrip*>(attached_tabstrip_);

  // Determine the horizontal move threshold. This is dependent on the width
  // of tabs. The smaller the tabs compared to the standard size, the smaller
  // the threshold.
  double unselected, selected;
  tab_strip->GetCurrentTabWidths(&unselected, &selected);
  double ratio = unselected / Tab::GetStandardSize().width();
  int threshold = static_cast<int>(ratio * kHorizontalMoveThreshold);

  std::vector<BaseTab*> tabs(drag_data_.size());
  for (size_t i = 0; i < drag_data_.size(); ++i)
    tabs[i] = drag_data_[i].attached_tab;

  bool did_layout = false;
  // Update the model, moving the TabContents from one index to another. Do this
  // only if we have moved a minimum distance since the last reorder (to prevent
  // jitter) or if this the first move and the tabs are not consecutive.
  if (abs(MajorAxisValue(screen_point, attached_tabstrip_) -
          last_move_screen_loc_) > threshold ||
      (initial_move_ && !AreTabsConsecutive())) {
    TabStripModel* attached_model = GetModel(attached_tabstrip_);
    gfx::Rect bounds = GetDraggedViewTabStripBounds(dragged_view_point);
    int to_index = GetInsertionIndexForDraggedBounds(bounds);
    TabContentsWrapper* last_contents =
        drag_data_[drag_data_.size() - 1].contents;
    int index_of_last_item =
          attached_model->GetIndexOfTabContents(last_contents);
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
        attached_model->GetIndexOfTabContents(last_contents)) {
      last_move_screen_loc_ = MajorAxisValue(screen_point, attached_tabstrip_);
    }
  }

  if (!did_layout) {
    attached_tabstrip_->LayoutDraggedTabsAt(
        tabs, source_tab_drag_data()->attached_tab, dragged_view_point,
        initial_move_);
  }

  initial_move_ = false;
}

TabDragController2::DetachPosition TabDragController2::GetDetachPosition(
    const gfx::Point& screen_point) {
  DCHECK(attached_tabstrip_);
  gfx::Point attached_point(screen_point);
  views::View::ConvertPointToView(NULL, attached_tabstrip_, &attached_point);
  if (attached_point.x() < 0)
    return DETACH_BEFORE;
  if (attached_point.x() >= attached_tabstrip_->width())
    return DETACH_AFTER;
  return DETACH_ABOVE_OR_BELOW;
}

DockInfo TabDragController2::GetDockInfoAtPoint(
    const gfx::Point& screen_point) {
  // TODO: add support for dock info.
  if (true) {
    return DockInfo();
  }

  if (attached_tabstrip_) {
    // If the mouse is over a tab strip, don't offer a dock position.
    return DockInfo();
  }

  if (dock_info_.IsValidForPoint(screen_point)) {
    // It's possible any given screen coordinate has multiple docking
    // positions. Check the current info first to avoid having the docking
    // position bounce around.
    return dock_info_;
  }

  //  gfx::NativeView dragged_hwnd = view_->GetWidget()->GetNativeView();
  // dock_windows_.insert(dragged_hwnd);
  DockInfo info = DockInfo::GetDockInfoAtPoint(screen_point, dock_windows_);
  // dock_windows_.erase(dragged_hwnd);
  return info;
}

TabStrip* TabDragController2::GetTabStripForPoint(
    const gfx::Point& screen_point) {
  gfx::NativeView dragged_view = NULL;
  if (is_dragging_window_) {
    dragged_view = attached_tabstrip_->GetWidget()->GetNativeView();
    dock_windows_.insert(dragged_view);
  }
  gfx::NativeWindow local_window =
      DockInfo::GetLocalProcessWindowAtPoint(screen_point, dock_windows_);
  if (dragged_view)
    dock_windows_.erase(dragged_view);
  TabStrip* tab_strip = GetTabStripForWindow(local_window);
  if (tab_strip && DoesTabStripContain(tab_strip, screen_point))
    return tab_strip;
  return is_dragging_window_ ? attached_tabstrip_ : NULL;
}

TabStrip* TabDragController2::GetTabStripForWindow(
    gfx::NativeWindow window) {
  if (!window)
    return NULL;
  BrowserView* browser = BrowserView::GetBrowserViewForNativeWindow(window);
  // We only allow drops on windows that have tabstrips.
  if (!browser ||
      !browser->browser()->SupportsWindowFeature(Browser::FEATURE_TABSTRIP))
    return NULL;

  TabStrip* other_tabstrip = static_cast<TabStrip*>(browser->tabstrip());
  TabStrip* tab_strip =
      attached_tabstrip_ ? attached_tabstrip_ : source_tabstrip_;
  DCHECK(tab_strip);
  if (!other_tabstrip->controller()->IsCompatibleWith(tab_strip))
    return NULL;
  return other_tabstrip;
}

bool TabDragController2::DoesTabStripContain(
    TabStrip* tabstrip,
    const gfx::Point& screen_point) const {
  static const int kVerticalDetachMagnetism = 15;
  // Make sure the specified screen point is actually within the bounds of the
  // specified tabstrip...
  gfx::Rect tabstrip_bounds = GetViewScreenBounds(tabstrip);
  if (screen_point.x() < tabstrip_bounds.right() &&
      screen_point.x() >= tabstrip_bounds.x()) {
    // TODO(beng): make this be relative to the start position of the mouse
    // for the source TabStrip.
    int upper_threshold = tabstrip_bounds.bottom() + kVerticalDetachMagnetism;
    int lower_threshold = tabstrip_bounds.y() - kVerticalDetachMagnetism;
    return screen_point.y() >= lower_threshold &&
        screen_point.y() <= upper_threshold;
  }
  return false;
}

void TabDragController2::Attach(TabStrip* attached_tabstrip,
                                const gfx::Point& screen_point) {
  DCHECK(!attached_tabstrip_);  // We should already have detached by the time
                                // we get here.

  attached_tabstrip_ = attached_tabstrip;

  std::vector<BaseTab*> tabs =
      GetTabsMatchingDraggedContents(attached_tabstrip_);

  if (tabs.empty()) {
    // Transitioning from detached to attached to a new tabstrip. Add tabs to
    // the new model.

    selection_model_before_attach_.Copy(attached_tabstrip->GetSelectionModel());

    // Inserting counts as a move. We don't want the tabs to jitter when the
    // user moves the tab immediately after attaching it.
    last_move_screen_loc_ = MajorAxisValue(screen_point, attached_tabstrip);

    // Figure out where to insert the tab based on the bounds of the dragged
    // representation and the ideal bounds of the other Tabs already in the
    // strip. ("ideal bounds" are stable even if the Tabs' actual bounds are
    // changing due to animation).
    gfx::Point tab_strip_point(screen_point);
    views::View::ConvertPointToView(NULL, attached_tabstrip_, &tab_strip_point);
    tab_strip_point.set_x(
        attached_tabstrip_->GetMirroredXInView(tab_strip_point.x()));
    tab_strip_point.Offset(-mouse_offset_.x(), -mouse_offset_.y());
    gfx::Rect bounds = GetDraggedViewTabStripBounds(tab_strip_point);
    int index = GetInsertionIndexForDraggedBounds(bounds);
    attached_tabstrip_->set_attaching_dragged_tab(true);
    for (size_t i = 0; i < drag_data_.size(); ++i) {
      int add_types = TabStripModel::ADD_NONE;
      if (drag_data_[i].pinned)
        add_types |= TabStripModel::ADD_PINNED;
      GetModel(attached_tabstrip_)->InsertTabContentsAt(
          index + i, drag_data_[i].contents, add_types);
    }
    attached_tabstrip_->set_attaching_dragged_tab(false);

    tabs = GetTabsMatchingDraggedContents(attached_tabstrip_);
  }
  DCHECK_EQ(tabs.size(), drag_data_.size());
  for (size_t i = 0; i < drag_data_.size(); ++i)
    drag_data_[i].attached_tab = tabs[i];

  attached_tabstrip_->StartedDraggingTabs(tabs);

  ResetSelection(GetModel(attached_tabstrip_));

  // The size of the dragged tab may have changed. Adjust the x offset so that
  // ratio of mouse_offset_ to original width is maintained.
  std::vector<BaseTab*> tabs_to_source(tabs);
  tabs_to_source.erase(tabs_to_source.begin() + source_tab_index_ + 1,
                       tabs_to_source.end());
  int new_x = attached_tabstrip_->GetSizeNeededForTabs(tabs_to_source) -
      tabs[source_tab_index_]->width() +
      static_cast<int>(offset_to_width_ratio_ *
                       tabs[source_tab_index_]->width());
  mouse_offset_.set_x(new_x);

  // Redirect all mouse events to the TabStrip so that the tab that originated
  // the drag can safely be deleted.
  static_cast<views::internal::RootView*>(
      attached_tabstrip_->GetWidget()->GetRootView())->SetMouseHandler(
          attached_tabstrip_);
}

void TabDragController2::Detach() {
  TabStripModel* attached_model = GetModel(attached_tabstrip_);
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    int index = attached_model->GetIndexOfTabContents(drag_data_[i].contents);
    DCHECK_NE(-1, index);

    // Hide the tab so that the user doesn't see it animate closed.
    drag_data_[i].attached_tab->SetVisible(false);

    attached_model->DetachTabContentsAt(index);

    // Detaching may end up deleting the tab, drop references to it.
    drag_data_[i].attached_tab = NULL;
  }

  if (!attached_model->empty()) {
    if (!selection_model_before_attach_.empty() &&
        selection_model_before_attach_.active() >= 0 &&
        selection_model_before_attach_.active() <
        attached_model->count()) {
      // Restore the selection.
      attached_model->SetSelectionFromModel(selection_model_before_attach_);
    } else if (attached_tabstrip_ == source_tabstrip_ &&
               !initial_selection_model_.empty()) {
      // First time detaching from the source tabstrip. Reset selection model to
      // initial_selection_model_. Before resetting though we have to remove all
      // the tabs from initial_selection_model_ as it was created with the tabs
      // still there.
      TabStripSelectionModel selection_model;
      selection_model.Copy(initial_selection_model_);
      for (DragData::const_reverse_iterator i = drag_data_.rbegin();
           i != drag_data_.rend(); ++i) {
        selection_model.DecrementFrom(i->source_model_index);
      }
      attached_model->SetSelectionFromModel(selection_model);
      // We may have cleared out the selection model. Only reset it if it
      // contains something.
      if (!selection_model.empty())
        attached_model->SetSelectionFromModel(selection_model);
    }
  }

  attached_tabstrip_ = NULL;
}

void TabDragController2::DetachIntoNewBrowserAndRunMoveLoop(
    const gfx::Point& screen_point) {
  if (GetModel(attached_tabstrip_)->count() ==
      static_cast<int>(drag_data_.size())) {
    // All the tabs in a browser are being dragged but all the tabs weren't
    // initially being dragged. For this to happen the user would have to
    // start dragging a set of tabs, the other tabs close, then detach.
    RunMoveLoop();
    return;
  }

  // Create a new browser to house the dragged tabs and have the OS run a move
  // loop.

  gfx::Point attached_point = GetAttachedDragPoint(screen_point);

  // Calculate the bounds for the tabs from the attached_tab_strip. We do this
  // so that the tabs don't change size when detached.
  std::vector<gfx::Rect> drag_bounds;
  std::vector<BaseTab*> attached_tabs;
  for (size_t i = 0; i < drag_data_.size(); ++i)
    attached_tabs.push_back(drag_data_[i].attached_tab);
  attached_tabstrip_->CalculateBoundsForDraggedTabs(attached_tabs,
                                                    &drag_bounds);
  for (size_t i = 0; i < drag_bounds.size(); ++i)
    drag_bounds[i].set_x(attached_point.x() + drag_bounds[i].x());

  Browser* browser = CreateBrowserForDrag(
      attached_tabstrip_, screen_point, &drag_bounds);
  attached_tabstrip_->ReleaseDragController();
  Detach();
  BrowserView* dragged_browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  dragged_browser_view->GetWidget()->SetVisibilityChangedAnimationsEnabled(
      false);
  Attach(static_cast<TabStrip*>(dragged_browser_view->tabstrip()),
         gfx::Point());
  attached_tabstrip_->OwnDragController(this);
  // TODO: come up with a cleaner way to do this.
  static_cast<TabStrip*>(attached_tabstrip_)->SetTabBoundsForDrag(
      drag_bounds);

  browser->window()->Show();
  browser->window()->Activate();
  dragged_browser_view->GetWidget()->SetVisibilityChangedAnimationsEnabled(
      true);
  RunMoveLoop();
}

void TabDragController2::RunMoveLoop() {
  move_loop_browser_view_ = GetAttachedBrowserView();
  move_loop_browser_view_->set_move_observer(this);
  is_dragging_window_ = true;
  bool destroyed = false;
  destroyed_ = &destroyed;
  // Running the move loop release mouse capture on windows, which triggers
  // destroying the drag loop. Release mouse capture ourself before this while
  // the Dragcontroller isn't owned by the TabStrip.
  attached_tabstrip_->ReleaseDragController();
  attached_tabstrip_->GetWidget()->ReleaseMouseCapture();
  attached_tabstrip_->OwnDragController(this);
  views::Widget::MoveLoopResult result =
      move_loop_browser_view_->GetWidget()->RunMoveLoop();
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_DRAG_LOOP_DONE,
      content::NotificationService::AllBrowserContextsAndSources(),
      content::NotificationService::NoDetails());

  if (destroyed)
    return;
  destroyed_ = NULL;
  move_loop_browser_view_->set_move_observer(NULL);
  move_loop_browser_view_ = NULL;
  is_dragging_window_ = false;
  waiting_for_run_loop_to_exit_ = false;
  if (end_run_loop_behavior_ == END_RUN_LOOP_CONTINUE_DRAGGING) {
    end_run_loop_behavior_ = END_RUN_LOOP_STOP_DRAGGING;
    if (tab_strip_to_attach_to_after_exit_) {
      Detach();
      gfx::Point screen_point(GetCursorScreenPoint());
      Attach(tab_strip_to_attach_to_after_exit_, screen_point);
      // Move the tabs into position.
      MoveAttached(screen_point);
      attached_tabstrip_->GetWidget()->Activate();
      tab_strip_to_attach_to_after_exit_ = NULL;
    }
    DCHECK(attached_tabstrip_);
    attached_tabstrip_->GetWidget()->SetMouseCapture(attached_tabstrip_);
  } else if (active_) {
    EndDrag(result == views::Widget::MOVE_LOOP_CANCELED);
  }
}

int TabDragController2::GetInsertionIndexForDraggedBounds(
    const gfx::Rect& dragged_bounds) const {
  // TODO: this is wrong. It needs to calculate the bounds after adjusting for
  // the tabs that will be inserted.
  int right_tab_x = 0;
  int index = -1;
  for (int i = 0; i < attached_tabstrip_->tab_count(); ++i) {
    const gfx::Rect& ideal_bounds = attached_tabstrip_->ideal_bounds(i);
    gfx::Rect left_half, right_half;
    ideal_bounds.SplitVertically(&left_half, &right_half);
    right_tab_x = right_half.right();
    if (dragged_bounds.x() >= right_half.x() &&
        dragged_bounds.x() < right_half.right()) {
      index = i + 1;
      break;
    } else if (dragged_bounds.x() >= left_half.x() &&
               dragged_bounds.x() < left_half.right()) {
      index = i;
      break;
    }
  }
  if (index == -1) {
    if (dragged_bounds.right() > right_tab_x) {
      index = GetModel(attached_tabstrip_)->count();
    } else {
      index = 0;
    }
  }

  if (!drag_data_[0].attached_tab) {
    // If 'attached_tab' is NULL, it means we're in the process of attaching and
    // don't need to constrain the index.
    return index;
  }

  int max_index = GetModel(attached_tabstrip_)->count() -
      static_cast<int>(drag_data_.size());
  return std::max(0, std::min(max_index, index));
}

gfx::Rect TabDragController2::GetDraggedViewTabStripBounds(
    const gfx::Point& tab_strip_point) {
  // attached_tab is NULL when inserting into a new tabstrip.
  if (source_tab_drag_data()->attached_tab) {
    return gfx::Rect(tab_strip_point.x(), tab_strip_point.y(),
                     source_tab_drag_data()->attached_tab->width(),
                     source_tab_drag_data()->attached_tab->height());
  }

  double sel_width, unselected_width;
  static_cast<TabStrip*>(attached_tabstrip_)->GetCurrentTabWidths(
      &sel_width, &unselected_width);
  return gfx::Rect(tab_strip_point.x(), tab_strip_point.y(),
                   static_cast<int>(sel_width),
                   Tab::GetStandardSize().height());
}

gfx::Point TabDragController2::GetAttachedDragPoint(
    const gfx::Point& screen_point) {
  DCHECK(attached_tabstrip_);  // The tab must be attached.

  gfx::Point tab_loc(screen_point);
  views::View::ConvertPointToView(NULL, attached_tabstrip_, &tab_loc);
  int x =
      attached_tabstrip_->GetMirroredXInView(tab_loc.x()) - mouse_offset_.x();
  int y = tab_loc.y() - mouse_offset_.y();

  // TODO: consider caching this.
  std::vector<BaseTab*> attached_tabs;
  for (size_t i = 0; i < drag_data_.size(); ++i)
    attached_tabs.push_back(drag_data_[i].attached_tab);

  int size = attached_tabstrip_->GetSizeNeededForTabs(attached_tabs);

  int max_x = attached_tabstrip_->width() - size;
  x = std::min(std::max(x, 0), max_x);
  y = 0;
  return gfx::Point(x, y);
}

std::vector<BaseTab*> TabDragController2::GetTabsMatchingDraggedContents(
    TabStrip* tabstrip) {
  TabStripModel* model = GetModel(attached_tabstrip_);
  std::vector<BaseTab*> tabs;
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    int model_index = model->GetIndexOfTabContents(drag_data_[i].contents);
    if (model_index == TabStripModel::kNoTab)
      return std::vector<BaseTab*>();
    tabs.push_back(tabstrip->GetBaseTabAtModelIndex(model_index));
  }
  return tabs;
}

void TabDragController2::EndDragImpl(EndDragType type) {
  DCHECK(active_);
  active_ = false;

  bring_to_front_timer_.Stop();

  if (is_dragging_window_) {
    // End the nested drag loop.
    GetAttachedBrowserView()->GetWidget()->EndMoveLoop();
    waiting_for_run_loop_to_exit_ = true;
  }

  // Hide the current dock controllers.
  for (size_t i = 0; i < dock_controllers_.size(); ++i) {
    // Be sure and clear the controller first, that way if Hide ends up
    // deleting the controller it won't call us back.
    dock_controllers_[i]->clear_controller();
    dock_controllers_[i]->Hide();
  }
  dock_controllers_.clear();
  dock_windows_.clear();

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
    RevertDrag();
  }  // else case the only tab we were dragging was deleted. Nothing to do.

  // Clear out drag data so we don't attempt to do anything with it.
  drag_data_.clear();

  TabStrip* owning_tabstrip =
      attached_tabstrip_ ? attached_tabstrip_ : source_tabstrip_;
  owning_tabstrip->DestroyDragController();
}

void TabDragController2::RevertDrag() {
  std::vector<BaseTab*> tabs;
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    if (drag_data_[i].contents) {
      // Contents is NULL if a tab was destroyed while the drag was under way.
      tabs.push_back(drag_data_[i].attached_tab);
      RevertDragAt(i);
    }
  }

  if (attached_tabstrip_ && attached_tabstrip_ == source_tabstrip_)
    attached_tabstrip_->StoppedDraggingTabs(tabs);

  if (initial_selection_model_.empty())
    ResetSelection(GetModel(source_tabstrip_));
  else
    GetModel(source_tabstrip_)->SetSelectionFromModel(initial_selection_model_);

  if (source_tabstrip_)
    source_tabstrip_->GetWidget()->Activate();
}

void TabDragController2::ResetSelection(TabStripModel* model) {
  DCHECK(model);
  TabStripSelectionModel selection_model;
  bool has_one_valid_tab = false;
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    // |contents| is NULL if a tab was deleted out from under us.
    if (drag_data_[i].contents) {
      int index = model->GetIndexOfTabContents(drag_data_[i].contents);
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

void TabDragController2::RevertDragAt(size_t drag_index) {
  DCHECK(started_drag_);

  TabDragData* data = &(drag_data_[drag_index]);
  int index =
      GetModel(attached_tabstrip_)->GetIndexOfTabContents(data->contents);
  if (attached_tabstrip_ != source_tabstrip_) {
    // The Tab was inserted into another TabStrip. We need to put it back into
    // the original one.
    GetModel(attached_tabstrip_)->DetachTabContentsAt(index);
    // TODO(beng): (Cleanup) seems like we should use Attach() for this
    //             somehow.
    GetModel(source_tabstrip_)->InsertTabContentsAt(
        data->source_model_index, data->contents,
        (data->pinned ? TabStripModel::ADD_PINNED : 0));
  } else {
    // The Tab was moved within the TabStrip where the drag was initiated.  Move
    // it back to the starting location.
    GetModel(source_tabstrip_)->MoveTabContentsAt(
        index, data->source_model_index, false);
  }
}

void TabDragController2::CompleteDrag() {
  DCHECK(started_drag_);

  if (attached_tabstrip_) {
    attached_tabstrip_->StoppedDraggingTabs(
        GetTabsMatchingDraggedContents(attached_tabstrip_));
  } else {
    NOTIMPLEMENTED();

    if (dock_info_.type() != DockInfo::NONE) {
      switch (dock_info_.type()) {
        case DockInfo::LEFT_OF_WINDOW:
          content::RecordAction(UserMetricsAction("DockingWindow_Left"));
          break;

        case DockInfo::RIGHT_OF_WINDOW:
          content::RecordAction(UserMetricsAction("DockingWindow_Right"));
          break;

        case DockInfo::BOTTOM_OF_WINDOW:
          content::RecordAction(UserMetricsAction("DockingWindow_Bottom"));
          break;

        case DockInfo::TOP_OF_WINDOW:
          content::RecordAction(UserMetricsAction("DockingWindow_Top"));
          break;

        case DockInfo::MAXIMIZE:
          content::RecordAction(UserMetricsAction("DockingWindow_Maximize"));
          break;

        case DockInfo::LEFT_HALF:
          content::RecordAction(UserMetricsAction("DockingWindow_LeftHalf"));
          break;

        case DockInfo::RIGHT_HALF:
          content::RecordAction(UserMetricsAction("DockingWindow_RightHalf"));
          break;

        case DockInfo::BOTTOM_HALF:
          content::RecordAction(UserMetricsAction("DockingWindow_BottomHalf"));
          break;

        default:
          NOTREACHED();
          break;
      }
    }
    // Compel the model to construct a new window for the detached TabContents.
    views::Widget* widget = source_tabstrip_->GetWidget();
    gfx::Rect window_bounds(widget->GetRestoredBounds());
    window_bounds.set_origin(GetWindowCreatePoint());
    // When modifying the following if statement, please make sure not to
    // introduce issue listed in http://crbug.com/6223 comment #11.
    bool rtl_ui = base::i18n::IsRTL();
    bool has_dock_position = (dock_info_.type() != DockInfo::NONE);
    if (rtl_ui && has_dock_position) {
      // Mirror X axis so the docked tab is aligned using the mouse click as
      // the top-right corner.
      window_bounds.set_x(window_bounds.x() - window_bounds.width());
    }
    Browser* new_browser =
        GetModel(source_tabstrip_)->delegate()->CreateNewStripWithContents(
            drag_data_[0].contents, window_bounds, dock_info_,
            widget->IsMaximized());
    TabStripModel* new_model = new_browser->tabstrip_model();
    new_model->SetTabPinned(
        new_model->GetIndexOfTabContents(drag_data_[0].contents),
        drag_data_[0].pinned);
    for (size_t i = 1; i < drag_data_.size(); ++i) {
      new_model->InsertTabContentsAt(
          static_cast<int>(i),
          drag_data_[i].contents,
          drag_data_[i].pinned ? TabStripModel::ADD_PINNED :
                                 TabStripModel::ADD_NONE);
    }
    ResetSelection(new_browser->tabstrip_model());
    new_browser->window()->Show();
  }
}

gfx::Point TabDragController2::GetCursorScreenPoint() const {
  // TODO(sky): see if we can convert to using Screen every where.
#if defined(OS_WIN) && !defined(USE_AURA)
  DWORD pos = GetMessagePos();
  return gfx::Point(pos);
#else
  return gfx::Screen::GetCursorScreenPoint();
#endif
}

gfx::Rect TabDragController2::GetViewScreenBounds(views::View* view) const {
  gfx::Point view_topleft;
  views::View::ConvertPointToScreen(view, &view_topleft);
  gfx::Rect view_screen_bounds = view->GetLocalBounds();
  view_screen_bounds.Offset(view_topleft.x(), view_topleft.y());
  return view_screen_bounds;
}

void TabDragController2::DockDisplayerDestroyed(
    DockDisplayer* controller) {
  DockWindows::iterator dock_i =
      dock_windows_.find(controller->popup_view());
  if (dock_i != dock_windows_.end())
    dock_windows_.erase(dock_i);
  else
    NOTREACHED();

  std::vector<DockDisplayer*>::iterator i =
      std::find(dock_controllers_.begin(), dock_controllers_.end(),
                controller);
  if (i != dock_controllers_.end())
    dock_controllers_.erase(i);
  else
    NOTREACHED();
}

void TabDragController2::BringWindowUnderMouseToFront() {
  gfx::NativeWindow window = dock_info_.window();
  if (!window) {
    gfx::NativeView dragged_view =
        attached_tabstrip_->GetWidget()->GetNativeView();
    dock_windows_.insert(dragged_view);
    window = DockInfo::GetLocalProcessWindowAtPoint(GetCursorScreenPoint(),
                                                    dock_windows_);
    dock_windows_.erase(dragged_view);
  }
  if (window) {
    views::Widget* widget = views::Widget::GetWidgetForNativeWindow(window);
    if (widget) {
      widget->StackAtTop();
      attached_tabstrip_->GetWidget()->StackAtTop();
    }
  }
}

// static
TabStripModel* TabDragController2::GetModel(TabStrip* tabstrip) {
  return static_cast<BrowserTabStripController*>(tabstrip->controller())->
      model();
}

BrowserView* TabDragController2::GetAttachedBrowserView() {
  return BrowserView::GetBrowserViewForNativeWindow(
      attached_tabstrip_->GetWidget()->GetNativeView());
}

bool TabDragController2::AreTabsConsecutive() {
  for (size_t i = 1; i < drag_data_.size(); ++i) {
    if (drag_data_[i - 1].source_model_index + 1 !=
        drag_data_[i].source_model_index) {
      return false;
    }
  }
  return true;
}

Browser* TabDragController2::CreateBrowserForDrag(
    TabStrip* source,
    const gfx::Point& screen_point,
    std::vector<gfx::Rect>* drag_bounds) {
  Browser* browser = Browser::Create(drag_data_[0].contents->profile());
  gfx::Point center(0, source->height() / 2);
  views::View::ConvertPointToWidget(source, &center);
  gfx::Rect new_bounds(source->GetWidget()->GetWindowScreenBounds());
  new_bounds.set_y(screen_point.y() - center.y());
  switch (GetDetachPosition(screen_point)) {
    case DETACH_BEFORE:
      new_bounds.set_x(screen_point.x() - center.x());
      new_bounds.Offset(-mouse_offset_.x(), 0);
      break;

    case DETACH_AFTER: {
      gfx::Point right_edge(source->width(), 0);
      views::View::ConvertPointToWidget(source, &right_edge);
      new_bounds.set_x(screen_point.x() - right_edge.x());
      new_bounds.Offset(drag_bounds->back().right() - mouse_offset_.x(), 0);
      int delta = (*drag_bounds)[0].x();
      for (size_t i = 0; i < drag_bounds->size(); ++i)
        (*drag_bounds)[i].Offset(-delta, 0);
      break;
    }

    default:
      break; // Nothing to do for DETACH_ABOVE_OR_BELOW.
  }

  browser->window()->SetBounds(new_bounds);
  return browser;
}
