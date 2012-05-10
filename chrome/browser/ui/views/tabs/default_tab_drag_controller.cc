// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/default_tab_drag_controller.h"

#include <math.h>
#include <set>

#include "base/callback.h"
#include "base/command_line.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/ui/app_modal_dialogs/message_box_handler.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/base_tab.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/dragged_tab_view.h"
#include "chrome/browser/ui/views/tabs/native_view_photobooth.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/browser/ui/views/tabs/touch_tab_strip_layout.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/chrome_switches.h"
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
#include "ui/gfx/canvas.h"
#include "ui/gfx/screen.h"
#include "ui/views/events/event.h"
#include "ui/views/widget/root_view.h"
#include "ui/views/widget/widget.h"

#if defined(USE_ASH)
#include "ash/shell.h"
#include "ash/wm/property_util.h"
#endif

using content::OpenURLParams;
using content::UserMetricsAction;
using content::WebContents;

static const int kHorizontalMoveThreshold = 16;  // Pixels.

// Distance from the next/previous stacked before before we consider the tab
// close enough to trigger moving.
static const int kStackedDistance = 36;

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
    canvas->sk_canvas()->drawRoundRect(
        outer_rect, SkIntToScalar(kRoundedRectRadius),
        SkIntToScalar(kRoundedRectRadius), paint);

    ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

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

void SetTrackedByWorkspace(gfx::NativeWindow window, bool value) {
#if defined(USE_ASH)
  ash::SetTrackedByWorkspace(window, value);
#endif
}

bool ShouldDetachIntoNewBrowser() {
#if defined(USE_AURA)
  return true;
#else
  return CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kTabBrowserDragging);
#endif
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// DockDisplayer

// DockDisplayer is responsible for giving the user a visual indication of a
// possible dock position (as represented by DockInfo). DockDisplayer shows
// a window with a DockView in it. Two animations are used that correspond to
// the state of DockInfo::in_enable_area.
class TabDragController::DockDisplayer : public ui::AnimationDelegate {
 public:
  DockDisplayer(TabDragController* controller, const DockInfo& info)
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

  virtual ~DockDisplayer() {
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

  // Resets the reference to the hosting TabDragController. This is
  // invoked when the TabDragController is destroyed.
  void clear_controller() { controller_ = NULL; }

  // NativeView of the window we create.
  gfx::NativeView popup_view() { return popup_view_; }

  // Starts the hide animation. When the window is closed the
  // TabDragController is notified by way of the DockDisplayerDestroyed
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

  virtual void AnimationProgressed(const ui::Animation* animation) OVERRIDE {
    UpdateLayeredAlpha();
  }

  virtual void AnimationEnded(const ui::Animation* animation) OVERRIDE {
    if (!hidden_)
      return;
    popup_->Close();
    delete this;
  }

 private:
  void UpdateLayeredAlpha() {
    double scale = in_enable_area_ ? 1 : .5;
    popup_->SetOpacity(static_cast<unsigned char>(animation_.GetCurrentValue() *
        scale * 255.0));
  }

  // TabDragController that created us.
  TabDragController* controller_;

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

TabDragController::TabDragData::TabDragData()
    : contents(NULL),
      original_delegate(NULL),
      source_model_index(-1),
      attached_tab(NULL),
      pinned(false) {
}

TabDragController::TabDragData::~TabDragData() {
}

///////////////////////////////////////////////////////////////////////////////
// TabDragController, public:

TabDragController::TabDragController()
    : detach_into_browser_(ShouldDetachIntoNewBrowser()),
      source_tabstrip_(NULL),
      attached_tabstrip_(NULL),
      source_tab_offset_(0),
      offset_to_width_ratio_(0),
      old_focused_view_(NULL),
      last_move_screen_loc_(0),
      started_drag_(false),
      active_(true),
      source_tab_index_(std::numeric_limits<size_t>::max()),
      initial_move_(true),
      move_only_(false),
      mouse_move_direction_(0),
      is_dragging_window_(false),
      end_run_loop_behavior_(END_RUN_LOOP_STOP_DRAGGING),
      waiting_for_run_loop_to_exit_(false),
      tab_strip_to_attach_to_after_exit_(NULL),
      move_loop_widget_(NULL),
      destroyed_(NULL) {
  instance_ = this;
}

TabDragController::~TabDragController() {
  if (instance_ == this)
    instance_ = NULL;

  if (destroyed_)
    *destroyed_ = true;

  if (move_loop_widget_) {
    move_loop_widget_->RemoveObserver(this);
    SetTrackedByWorkspace(move_loop_widget_->GetNativeView(), true);
  }

  if (source_tabstrip_ && detach_into_browser_)
    GetModel(source_tabstrip_)->RemoveObserver(this);

  MessageLoopForUI::current()->RemoveObserver(this);

  // Need to delete the view here manually _before_ we reset the dragged
  // contents to NULL, otherwise if the view is animating to its destination
  // bounds, it won't be able to clean up properly since its cleanup routine
  // uses GetIndexForDraggedContents, which will be invalid.
  view_.reset(NULL);

  // Reset the delegate of the dragged WebContents. This ends up doing nothing
  // if the drag was completed.
  if (!detach_into_browser_)
    ResetDelegates();
}

void TabDragController::Init(
    TabStrip* source_tabstrip,
    BaseTab* source_tab,
    const std::vector<BaseTab*>& tabs,
    const gfx::Point& mouse_offset,
    int source_tab_offset,
    const TabStripSelectionModel& initial_selection_model,
    bool move_only) {
  DCHECK(!tabs.empty());
  DCHECK(std::find(tabs.begin(), tabs.end(), source_tab) != tabs.end());
  source_tabstrip_ = source_tabstrip;
  source_tab_offset_ = source_tab_offset;
  start_screen_point_ = GetCursorScreenPoint();
  mouse_offset_ = mouse_offset;
  move_only_ = move_only;
  last_screen_point_ = start_screen_point_;
  if (move_only_) {
    last_move_screen_loc_ = start_screen_point_.x();
    initial_tab_positions_ = source_tabstrip->GetTabXCoordinates();
  }
  if (detach_into_browser_)
    GetModel(source_tabstrip_)->AddObserver(this);

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
  InitWindowCreatePoint();
  initial_selection_model_.Copy(initial_selection_model);
}

// static
bool TabDragController::IsAttachedTo(TabStrip* tab_strip) {
  return (instance_ && instance_->active() &&
          instance_->attached_tabstrip() == tab_strip);
}

// static
bool TabDragController::IsActive() {
  return instance_ && instance_->active();
}

void TabDragController::Drag() {
  bring_to_front_timer_.Stop();
  move_stacked_timer_.Stop();

  if (waiting_for_run_loop_to_exit_)
    return;

  if (!started_drag_) {
    if (!CanStartDrag())
      return;  // User hasn't dragged far enough yet.

    started_drag_ = true;
    SaveFocus();
    Attach(source_tabstrip_, gfx::Point());
    if (detach_into_browser_ && static_cast<int>(drag_data_.size()) ==
        GetModel(source_tabstrip_)->count()) {
      RunMoveLoop();  // Runs a nested loop, returning when done.
      return;
    }
  }

  ContinueDragging();
}

void TabDragController::EndDrag(bool canceled) {
  EndDragImpl(canceled && source_tabstrip_ ? CANCELED : NORMAL);
}

void TabDragController::InitTabDragData(BaseTab* tab,
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

  if (!detach_into_browser_) {
    drag_data->original_delegate =
        drag_data->contents->web_contents()->GetDelegate();
    drag_data->contents->web_contents()->SetDelegate(this);
  }
}

///////////////////////////////////////////////////////////////////////////////
// TabDragController, PageNavigator implementation:

WebContents* TabDragController::OpenURLFromTab(
    WebContents* source,
    const OpenURLParams& params) {
  if (source_tab_drag_data()->original_delegate) {
    OpenURLParams forward_params = params;
    if (params.disposition == CURRENT_TAB)
      forward_params.disposition = NEW_WINDOW;

    return source_tab_drag_data()->original_delegate->OpenURLFromTab(
        source, forward_params);
  }
  return NULL;
}

///////////////////////////////////////////////////////////////////////////////
// TabDragController, content::WebContentsDelegate implementation:

void TabDragController::NavigationStateChanged(const WebContents* source,
                                               unsigned changed_flags) {
  if (attached_tabstrip_) {
    for (size_t i = 0; i < drag_data_.size(); ++i) {
      if (drag_data_[i].contents->web_contents() == source) {
        // Pass the NavigationStateChanged call to the original delegate so
        // that the title is updated. Do this only when we are attached as
        // otherwise the Tab isn't in the TabStrip.
        drag_data_[i].original_delegate->NavigationStateChanged(source,
                                                                changed_flags);
        break;
      }
    }
  }
  if (view_.get())
    view_->Update();
}

void TabDragController::AddNewContents(WebContents* source,
                                       WebContents* new_contents,
                                       WindowOpenDisposition disposition,
                                       const gfx::Rect& initial_pos,
                                       bool user_gesture) {
  DCHECK_NE(CURRENT_TAB, disposition);

  // Theoretically could be called while dragging if the page tries to
  // spawn a window. Route this message back to the browser in most cases.
  if (source_tab_drag_data()->original_delegate) {
    source_tab_drag_data()->original_delegate->AddNewContents(
        source, new_contents, disposition, initial_pos, user_gesture);
  }
}

void TabDragController::LoadingStateChanged(WebContents* source) {
  // It would be nice to respond to this message by changing the
  // screen shot in the dragged tab.
  if (view_.get())
    view_->Update();
}

bool TabDragController::ShouldSuppressDialogs() {
  // When a dialog is about to be shown we revert the drag. Otherwise a modal
  // dialog might appear and attempt to parent itself to a hidden tabcontents.
  EndDragImpl(CANCELED);
  return false;
}

content::JavaScriptDialogCreator*
TabDragController::GetJavaScriptDialogCreator() {
  return GetJavaScriptDialogCreatorInstance();
}

///////////////////////////////////////////////////////////////////////////////
// TabDragController, content::NotificationObserver implementation:

void TabDragController::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(type, content::NOTIFICATION_WEB_CONTENTS_DESTROYED);
  WebContents* destroyed_contents = content::Source<WebContents>(source).ptr();
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    if (drag_data_[i].contents->web_contents() == destroyed_contents) {
      // One of the tabs we're dragging has been destroyed. Cancel the drag.
      if (destroyed_contents->GetDelegate() == this)
        destroyed_contents->SetDelegate(NULL);
      drag_data_[i].contents = NULL;
      drag_data_[i].original_delegate = NULL;
      EndDragImpl(TAB_DESTROYED);
      return;
    }
  }
  // If we get here it means we got notification for a tab we don't know about.
  NOTREACHED();
}

///////////////////////////////////////////////////////////////////////////////
// TabDragController, MessageLoop::Observer implementation:

base::EventStatus TabDragController::WillProcessEvent(
    const base::NativeEvent& event) {
  return base::EVENT_CONTINUE;
}

void TabDragController::DidProcessEvent(const base::NativeEvent& event) {
  // If the user presses ESC during a drag, we need to abort and revert things
  // to the way they were. This is the most reliable way to do this since no
  // single view or window reliably receives events throughout all the various
  // kinds of tab dragging.
  if (ui::EventTypeFromNative(event) == ui::ET_KEY_PRESSED &&
      ui::KeyboardCodeFromNative(event) == ui::VKEY_ESCAPE) {
    EndDrag(true);
  }
}

void TabDragController::OnWidgetMoved(views::Widget* widget) {
  Drag();
}

void TabDragController::TabStripEmpty() {
  DCHECK(detach_into_browser_);
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

gfx::Point TabDragController::GetWindowCreatePoint() const {
  gfx::Point cursor_point = GetCursorScreenPoint();
  if (dock_info_.type() != DockInfo::NONE && dock_info_.in_enable_area()) {
    // If we're going to dock, we need to return the exact coordinate,
    // otherwise we may attempt to maximize on the wrong monitor.
    return cursor_point;
  }
  // If the cursor is outside the monitor area, move it inside. For example,
  // dropping a tab onto the task bar on Windows produces this situation.
  gfx::Rect work_area = gfx::Screen::GetMonitorNearestPoint(
      cursor_point).work_area();
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

void TabDragController::UpdateDockInfo(const gfx::Point& screen_point) {
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

void TabDragController::SaveFocus() {
  DCHECK(!old_focused_view_);  // This should only be invoked once.
  DCHECK(source_tabstrip_);
  old_focused_view_ = source_tabstrip_->GetFocusManager()->GetFocusedView();
  source_tabstrip_->GetFocusManager()->SetFocusedView(source_tabstrip_);
}

void TabDragController::RestoreFocus() {
  if (old_focused_view_ && attached_tabstrip_ == source_tabstrip_)
    old_focused_view_->GetFocusManager()->SetFocusedView(old_focused_view_);
  old_focused_view_ = NULL;
}

bool TabDragController::CanStartDrag() const {
  // Determine if the mouse has moved beyond a minimum elasticity distance in
  // any direction from the starting point.
  static const int kMinimumDragDistance = 10;
  gfx::Point screen_point = GetCursorScreenPoint();
  int x_offset = abs(screen_point.x() - start_screen_point_.x());
  int y_offset = abs(screen_point.y() - start_screen_point_.y());
  return sqrt(pow(static_cast<float>(x_offset), 2) +
              pow(static_cast<float>(y_offset), 2)) > kMinimumDragDistance;
}

void TabDragController::ContinueDragging() {
  DCHECK(!detach_into_browser_ || attached_tabstrip_);

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

  TabStrip* target_tabstrip = move_only_ ?
      source_tabstrip_ : GetTabStripForPoint(screen_point);
  bool tab_strip_changed = (target_tabstrip != attached_tabstrip_);

  if (tab_strip_changed) {
    if (detach_into_browser_ &&
        DragBrowserToNewTabStrip(target_tabstrip, screen_point) ==
        DRAG_BROWSER_RESULT_STOP) {
      return;
    } else if (!detach_into_browser_) {
      if (attached_tabstrip_)
        Detach();
      if (target_tabstrip)
        Attach(target_tabstrip, screen_point);
    }
  }
  if (view_.get() || is_dragging_window_) {
    bring_to_front_timer_.Start(FROM_HERE,
        base::TimeDelta::FromMilliseconds(kBringToFrontDelay), this,
        &TabDragController::BringWindowUnderMouseToFront);
  }

  UpdateDockInfo(screen_point);

  if (!is_dragging_window_) {
    if (attached_tabstrip_) {
      if (move_only_) {
        DragActiveTabStacked(screen_point);
      } else {
        MoveAttached(screen_point);
        if (tab_strip_changed) {
          // Move the corresponding window to the front. We do this after the
          // move as on windows activate triggers a synchronous paint.
          attached_tabstrip_->GetWidget()->Activate();
        }
      }
    } else {
      MoveDetached(screen_point);
    }
  }
}

TabDragController::DragBrowserResultType
TabDragController::DragBrowserToNewTabStrip(
    TabStrip* target_tabstrip,
    const gfx::Point& screen_point) {
  if (!target_tabstrip) {
    DetachIntoNewBrowserAndRunMoveLoop(screen_point);
    return DRAG_BROWSER_RESULT_STOP;
  }
  if (is_dragging_window_) {
#if defined(USE_ASH)
    // ReleaseMouseCapture() is going to result in calling back to us (because
    // it results in a move). That'll cause all sorts of problems.  Reset the
    // observer so we don't get notified and process the event.
    move_loop_widget_->RemoveObserver(this);
    move_loop_widget_ = NULL;
#endif
    views::Widget* browser_widget = GetAttachedBrowserWidget();
    // Need to release the drag controller before starting the move loop as it's
    // going to trigger capture lost, which cancels drag.
    attached_tabstrip_->ReleaseDragController();
    target_tabstrip->OwnDragController(this);
    // Disable animations so that we don't see a close animation on aero.
    browser_widget->SetVisibilityChangedAnimationsEnabled(false);
    browser_widget->ReleaseMouseCapture();
    // EndMoveLoop is going to snap the window back to its original location.
    // Hide it so users don't see this.
    browser_widget->Hide();
    browser_widget->EndMoveLoop();

    // Ideally we would always swap the tabs now, but on windows it seems that
    // running the move loop implicitly activates the window when done, leading
    // to all sorts of flicker. So, on windows, instead we process the move
    // after the loop completes. But on chromeos, we can do tab swapping now to
    // avoid the tab flashing issue(crbug.com/116329).
#if defined(USE_ASH)
    is_dragging_window_ = false;
    Detach();
    gfx::Point screen_point(GetCursorScreenPoint());
    Attach(target_tabstrip, screen_point);
    // Move the tabs into position.
    MoveAttached(screen_point);
    attached_tabstrip_->GetWidget()->Activate();
#else
    tab_strip_to_attach_to_after_exit_ = target_tabstrip;
#endif

    waiting_for_run_loop_to_exit_ = true;
    end_run_loop_behavior_ = END_RUN_LOOP_CONTINUE_DRAGGING;
    return DRAG_BROWSER_RESULT_STOP;
  }
  Detach();
  Attach(target_tabstrip, screen_point);
  return DRAG_BROWSER_RESULT_CONTINUE;
}

void TabDragController::DragActiveTabStacked(
    const gfx::Point& screen_point) {
  if (attached_tabstrip_->tab_count() !=
      static_cast<int>(initial_tab_positions_.size()))
    return;  // TODO: should cancel drag if this happens.

  int delta = screen_point.x() - start_screen_point_.x();
  attached_tabstrip_->DragActiveTab(initial_tab_positions_, delta);
}

void TabDragController::MoveAttachedToNextStackedIndex() {
  int index = attached_tabstrip_->touch_layout_->active_index();
  if (index + 1 >= attached_tabstrip_->tab_count())
    return;

  GetModel(attached_tabstrip_)->MoveSelectedTabsTo(index + 1);
  StartMoveStackedTimerIfNecessary(kMoveAttachedSubsequentDelay);
}

void TabDragController::MoveAttachedToPreviousStackedIndex() {
  int index = attached_tabstrip_->touch_layout_->active_index();
  if (index <= attached_tabstrip_->GetMiniTabCount())
    return;

  GetModel(attached_tabstrip_)->MoveSelectedTabsTo(index - 1);
  StartMoveStackedTimerIfNecessary(kMoveAttachedSubsequentDelay);
}

void TabDragController::MoveAttached(const gfx::Point& screen_point) {
  DCHECK(attached_tabstrip_);
  DCHECK(!view_.get());
  DCHECK(!is_dragging_window_);

  int move_delta = screen_point.x() - last_screen_point_.x();
  if (move_delta > 0)
    mouse_move_direction_ |= kMovedMouseRight;
  else if (move_delta < 0)
    mouse_move_direction_ |= kMovedMouseLeft;
  last_screen_point_ = screen_point;

  gfx::Point dragged_view_point = GetAttachedDragPoint(screen_point);

  // Determine the horizontal move threshold. This is dependent on the width
  // of tabs. The smaller the tabs compared to the standard size, the smaller
  // the threshold.
  int threshold = kHorizontalMoveThreshold;
  if (!attached_tabstrip_->touch_layout_.get()) {
    double unselected, selected;
    attached_tabstrip_->GetCurrentTabWidths(&unselected, &selected);
    double ratio = unselected / Tab::GetStandardSize().width();
    threshold = static_cast<int>(ratio * kHorizontalMoveThreshold);
  }
  // else case: touch tabs never shrink.

  std::vector<BaseTab*> tabs(drag_data_.size());
  for (size_t i = 0; i < drag_data_.size(); ++i)
    tabs[i] = drag_data_[i].attached_tab;

  bool did_layout = false;
  // Update the model, moving the WebContents from one index to another. Do this
  // only if we have moved a minimum distance since the last reorder (to prevent
  // jitter) or if this the first move and the tabs are not consecutive.
  if ((abs(screen_point.x() - last_move_screen_loc_) > threshold ||
        (initial_move_ && !AreTabsConsecutive()))) {
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
      last_move_screen_loc_ = screen_point.x();
    }
  }

  if (!did_layout) {
    attached_tabstrip_->LayoutDraggedTabsAt(
        tabs, source_tab_drag_data()->attached_tab, dragged_view_point,
        initial_move_);
  }

  StartMoveStackedTimerIfNecessary(kMoveAttachedInitialDelay);

  initial_move_ = false;
}

void TabDragController::MoveDetached(const gfx::Point& screen_point) {
  DCHECK(!attached_tabstrip_);
  DCHECK(view_.get());
  DCHECK(!is_dragging_window_);

  // Move the View. There are no changes to the model if we're detached.
  view_->MoveTo(screen_point);
}

void TabDragController::StartMoveStackedTimerIfNecessary(int delay_ms) {
  DCHECK(attached_tabstrip_);

  TouchTabStripLayout* touch_layout = attached_tabstrip_->touch_layout_.get();
  if (!touch_layout)
    return;

  gfx::Point screen_point = GetCursorScreenPoint();
  gfx::Point dragged_view_point = GetAttachedDragPoint(screen_point);
  gfx::Rect bounds = GetDraggedViewTabStripBounds(dragged_view_point);
  int index = touch_layout->active_index();
  if (ShouldDragToNextStackedTab(bounds, index)) {
    move_stacked_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(delay_ms), this,
        &TabDragController::MoveAttachedToNextStackedIndex);
  } else if (ShouldDragToPreviousStackedTab(bounds, index)) {
    move_stacked_timer_.Start(
        FROM_HERE,
        base::TimeDelta::FromMilliseconds(delay_ms), this,
        &TabDragController::MoveAttachedToPreviousStackedIndex);
  }
}

TabDragController::DetachPosition TabDragController::GetDetachPosition(
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

DockInfo TabDragController::GetDockInfoAtPoint(const gfx::Point& screen_point) {
  // TODO: add support for dock info when |detach_into_browser_| is true.
  if (attached_tabstrip_ || detach_into_browser_) {
    // If the mouse is over a tab strip, don't offer a dock position.
    return DockInfo();
  }

  if (dock_info_.IsValidForPoint(screen_point)) {
    // It's possible any given screen coordinate has multiple docking
    // positions. Check the current info first to avoid having the docking
    // position bounce around.
    return dock_info_;
  }

  gfx::NativeView dragged_view = view_->GetWidget()->GetNativeView();
  dock_windows_.insert(dragged_view);
  DockInfo info = DockInfo::GetDockInfoAtPoint(screen_point, dock_windows_);
  dock_windows_.erase(dragged_view);
  return info;
}

TabStrip* TabDragController::GetTabStripForPoint(
    const gfx::Point& screen_point) {
  gfx::NativeView dragged_view = NULL;
  if (view_.get())
    dragged_view = view_->GetWidget()->GetNativeView();
  else if (is_dragging_window_)
    dragged_view = attached_tabstrip_->GetWidget()->GetNativeView();
  if (dragged_view)
    dock_windows_.insert(dragged_view);
  gfx::NativeWindow local_window =
      DockInfo::GetLocalProcessWindowAtPoint(screen_point, dock_windows_);
  if (dragged_view)
    dock_windows_.erase(dragged_view);
  TabStrip* tab_strip = GetTabStripForWindow(local_window);
  if (tab_strip && DoesTabStripContain(tab_strip, screen_point))
    return tab_strip;
  return is_dragging_window_ ? attached_tabstrip_ : NULL;
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

void TabDragController::Attach(TabStrip* attached_tabstrip,
                               const gfx::Point& screen_point) {
  DCHECK(!attached_tabstrip_);  // We should already have detached by the time
                                // we get here.

  attached_tabstrip_ = attached_tabstrip;

  // And we don't need the dragged view.
  view_.reset();

  std::vector<BaseTab*> tabs =
      GetTabsMatchingDraggedContents(attached_tabstrip_);

  if (tabs.empty()) {
    // Transitioning from detached to attached to a new tabstrip. Add tabs to
    // the new model.

    selection_model_before_attach_.Copy(attached_tabstrip->GetSelectionModel());

    if (!detach_into_browser_) {
      // Remove ourselves as the delegate now that the dragged WebContents is
      // being inserted back into a Browser.
      for (size_t i = 0; i < drag_data_.size(); ++i) {
        drag_data_[i].contents->web_contents()->SetDelegate(NULL);
        drag_data_[i].original_delegate = NULL;
      }

      // Return the WebContents to normalcy.
      source_dragged_contents()->web_contents()->SetCapturingContents(false);
    }

    // Inserting counts as a move. We don't want the tabs to jitter when the
    // user moves the tab immediately after attaching it.
    last_move_screen_loc_ = screen_point.x();

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
    for (size_t i = 0; i < drag_data_.size(); ++i) {
      int add_types = TabStripModel::ADD_NONE;
      if (attached_tabstrip_->touch_layout_.get()) {
        // TouchTabStripLayout positions relative to the active tab, if we don't
        // add the tab as active things bounce around.
        DCHECK_EQ(1u, drag_data_.size());
        add_types |= TabStripModel::ADD_ACTIVE;
      }
      if (drag_data_[i].pinned)
        add_types |= TabStripModel::ADD_PINNED;
      GetModel(attached_tabstrip_)->InsertTabContentsAt(
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
  if (detach_into_browser_ || attached_tabstrip_ == source_tabstrip_) {
    static_cast<views::internal::RootView*>(
        attached_tabstrip_->GetWidget()->GetRootView())->SetMouseHandler(
            attached_tabstrip_);
  }
}

void TabDragController::Detach() {
  mouse_move_direction_ = kMovedMouseLeft | kMovedMouseRight;

  // Prevent the WebContents HWND from being hidden by any of the model
  // operations performed during the drag.
  if (!detach_into_browser_)
    source_dragged_contents()->web_contents()->SetCapturingContents(true);

  std::vector<gfx::Rect> drag_bounds = CalculateBoundsForDraggedTabs(0);
  TabStripModel* attached_model = GetModel(attached_tabstrip_);
  std::vector<TabRendererData> tab_data;
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    tab_data.push_back(drag_data_[i].attached_tab->data());
    int index = attached_model->GetIndexOfTabContents(drag_data_[i].contents);
    DCHECK_NE(-1, index);

    // Hide the tab so that the user doesn't see it animate closed.
    drag_data_[i].attached_tab->SetVisible(false);

    attached_model->DetachTabContentsAt(index);

    // Detaching resets the delegate, but we still want to be the delegate.
    if (!detach_into_browser_)
      drag_data_[i].contents->web_contents()->SetDelegate(this);

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
      // We may have cleared out the selection model. Only reset it if it
      // contains something.
      if (!selection_model.empty())
        attached_model->SetSelectionFromModel(selection_model);
    }
  } else if (!detach_into_browser_) {
    HideFrame();
  }

  // Create the dragged view.
  if (!detach_into_browser_)
    CreateDraggedView(tab_data, drag_bounds);

  attached_tabstrip_->DraggedTabsDetached();
  attached_tabstrip_ = NULL;
}

void TabDragController::DetachIntoNewBrowserAndRunMoveLoop(
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
  std::vector<gfx::Rect> drag_bounds =
      CalculateBoundsForDraggedTabs(attached_point.x());

  Browser* browser = CreateBrowserForDrag(
      attached_tabstrip_, screen_point, &drag_bounds);
  attached_tabstrip_->ReleaseDragController();
  Detach();
  BrowserView* dragged_browser_view =
      BrowserView::GetBrowserViewForBrowser(browser);
  dragged_browser_view->GetWidget()->SetVisibilityChangedAnimationsEnabled(
      false);
  Attach(dragged_browser_view->tabstrip(), gfx::Point());
  attached_tabstrip_->OwnDragController(this);
  // TODO: come up with a cleaner way to do this.
  attached_tabstrip_->SetTabBoundsForDrag(drag_bounds);

  browser->window()->Show();
  browser->window()->Activate();
  dragged_browser_view->GetWidget()->SetVisibilityChangedAnimationsEnabled(
      true);
  RunMoveLoop();
}

void TabDragController::RunMoveLoop() {
  move_loop_widget_ = GetAttachedBrowserWidget();
  move_loop_widget_->AddObserver(this);
  is_dragging_window_ = true;
  bool destroyed = false;
  destroyed_ = &destroyed;
  // Running the move loop releases mouse capture on windows, which triggers
  // destroying the drag loop. Release mouse capture ourself before this while
  // the DragController isn't owned by the TabStrip.
  attached_tabstrip_->ReleaseDragController();
  attached_tabstrip_->GetWidget()->ReleaseMouseCapture();
  attached_tabstrip_->OwnDragController(this);
  views::Widget::MoveLoopResult result = move_loop_widget_->RunMoveLoop();
  content::NotificationService::current()->Notify(
      chrome::NOTIFICATION_TAB_DRAG_LOOP_DONE,
      content::NotificationService::AllBrowserContextsAndSources(),
      content::NotificationService::NoDetails());

  if (destroyed)
    return;
  destroyed_ = NULL;
  // Under chromeos we immediately set the |move_loop_widget_| to NULL.
  if (move_loop_widget_) {
    move_loop_widget_->RemoveObserver(this);
    move_loop_widget_ = NULL;
  }
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

int TabDragController::GetInsertionIndexFrom(const gfx::Rect& dragged_bounds,
                                             int start,
                                             int delta) const {
  for (int i = start, tab_count = attached_tabstrip_->tab_count();
       i >= 0 && i < tab_count; i += delta) {
    const gfx::Rect& ideal_bounds = attached_tabstrip_->ideal_bounds(i);
    gfx::Rect left_half, right_half;
    ideal_bounds.SplitVertically(&left_half, &right_half);
    if (dragged_bounds.x() >= right_half.x() &&
        dragged_bounds.x() < right_half.right()) {
      return i + 1;
    } else if (dragged_bounds.x() >= left_half.x() &&
               dragged_bounds.x() < left_half.right()) {
      return i;
    }
  }
  return -1;
}

int TabDragController::GetInsertionIndexForDraggedBounds(
    const gfx::Rect& dragged_bounds) const {
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
    index = GetInsertionIndexFrom(dragged_bounds, 0, 1);
  }
  if (index == -1) {
    int tab_count = attached_tabstrip_->tab_count();
    int right_tab_x = tab_count == 0 ? 0 :
        attached_tabstrip_->ideal_bounds(tab_count - 1).right();
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
  return dragged_bounds.x() >= mid_x;
}

bool TabDragController::ShouldDragToPreviousStackedTab(
    const gfx::Rect& dragged_bounds,
    int index) const {
  if (index - 1 < attached_tabstrip_->GetMiniTabCount() ||
      !attached_tabstrip_->touch_layout_->IsStacked(index - 1) ||
      (mouse_move_direction_ & kMovedMouseLeft) == 0)
    return false;

  int active_x = attached_tabstrip_->ideal_bounds(index).x();
  int previous_x = attached_tabstrip_->ideal_bounds(index - 1).x();
  int mid_x = std::max(previous_x + kStackedDistance,
                       active_x - (active_x - previous_x) / 4);
  return dragged_bounds.x() <= mid_x;
}

int TabDragController::GetInsertionIndexForDraggedBoundsStacked(
    const gfx::Rect& dragged_bounds) const {
  TouchTabStripLayout* touch_layout = attached_tabstrip_->touch_layout_.get();
  int active_index = touch_layout->active_index();
  // Search from the active index to the front of the tabstrip. Do this as tabs
  // overlap each other from the active index.
  int index = GetInsertionIndexFrom(dragged_bounds, active_index, -1);
  if (index != active_index)
    return index;
  if (index == -1)
    return GetInsertionIndexFrom(dragged_bounds, active_index + 1, 1);

  // The position to drag to corresponds to the active tab. If the next/previous
  // tab is stacked, then shorten the distance used to determine insertion
  // bounds. We do this as GetInsertionIndexFrom() uses the bounds of the
  // tabs. When tabs are stacked the next/previous tab is on top of the tab.
  if (active_index + 1 < attached_tabstrip_->tab_count() &&
      touch_layout->IsStacked(active_index + 1)) {
    index = GetInsertionIndexFrom(dragged_bounds, active_index + 1, 1);
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

  double sel_width, unselected_width;
  attached_tabstrip_->GetCurrentTabWidths(&sel_width, &unselected_width);
  return gfx::Rect(tab_strip_point.x(), tab_strip_point.y(),
                   static_cast<int>(sel_width),
                   Tab::GetStandardSize().height());
}

gfx::Point TabDragController::GetAttachedDragPoint(
    const gfx::Point& screen_point) {
  DCHECK(attached_tabstrip_);  // The tab must be attached.

  gfx::Point tab_loc(screen_point);
  views::View::ConvertPointToView(NULL, attached_tabstrip_, &tab_loc);
  int x =
      attached_tabstrip_->GetMirroredXInView(tab_loc.x()) - mouse_offset_.x();

  // TODO: consider caching this.
  std::vector<BaseTab*> attached_tabs;
  for (size_t i = 0; i < drag_data_.size(); ++i)
    attached_tabs.push_back(drag_data_[i].attached_tab);
  int size = attached_tabstrip_->GetSizeNeededForTabs(attached_tabs);
  int max_x = attached_tabstrip_->width() - size;
  return gfx::Point(std::min(std::max(x, 0), max_x), 0);
}

std::vector<BaseTab*> TabDragController::GetTabsMatchingDraggedContents(
    TabStrip* tabstrip) {
  TabStripModel* model = GetModel(attached_tabstrip_);
  std::vector<BaseTab*> tabs;
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    int model_index = model->GetIndexOfTabContents(drag_data_[i].contents);
    if (model_index == TabStripModel::kNoTab)
      return std::vector<BaseTab*>();
    tabs.push_back(tabstrip->tab_at(model_index));
  }
  return tabs;
}

std::vector<gfx::Rect> TabDragController::CalculateBoundsForDraggedTabs(
    int x_offset) {
  std::vector<gfx::Rect> drag_bounds;
  std::vector<BaseTab*> attached_tabs;
  for (size_t i = 0; i < drag_data_.size(); ++i)
    attached_tabs.push_back(drag_data_[i].attached_tab);
  attached_tabstrip_->CalculateBoundsForDraggedTabs(attached_tabs,
                                                    &drag_bounds);
  if (x_offset != 0) {
    for (size_t i = 0; i < drag_bounds.size(); ++i)
      drag_bounds[i].set_x(drag_bounds[i].x() + x_offset);
  }
  return drag_bounds;
}

void TabDragController::EndDragImpl(EndDragType type) {
  DCHECK(active_);
  active_ = false;

  bring_to_front_timer_.Stop();
  move_stacked_timer_.Stop();

  if (is_dragging_window_) {
    if (type == NORMAL || (type == TAB_DESTROYED && drag_data_.size() > 1))
      SetTrackedByWorkspace(GetAttachedBrowserWidget()->GetNativeView(), true);

    // End the nested drag loop.
    GetAttachedBrowserWidget()->EndMoveLoop();
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

  if (!detach_into_browser_)
    ResetDelegates();

  // Clear out drag data so we don't attempt to do anything with it.
  drag_data_.clear();

  TabStrip* owning_tabstrip = (attached_tabstrip_ && detach_into_browser_) ?
      attached_tabstrip_ : source_tabstrip_;
  owning_tabstrip->DestroyDragController();
}

void TabDragController::RevertDrag() {
  std::vector<BaseTab*> tabs;
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    if (drag_data_[i].contents) {
      // Contents is NULL if a tab was destroyed while the drag was under way.
      tabs.push_back(drag_data_[i].attached_tab);
      RevertDragAt(i);
    }
  }

  bool restore_frame = !detach_into_browser_ &&
                       attached_tabstrip_ != source_tabstrip_;
  if (attached_tabstrip_) {
    if (attached_tabstrip_ == source_tabstrip_)
      source_tabstrip_->StoppedDraggingTabs(tabs);
    else
      attached_tabstrip_->DraggedTabsDetached();
  }

  if (initial_selection_model_.empty())
    ResetSelection(GetModel(source_tabstrip_));
  else
    GetModel(source_tabstrip_)->SetSelectionFromModel(initial_selection_model_);

  // If we're not attached to any TabStrip, or attached to some other TabStrip,
  // we need to restore the bounds of the original TabStrip's frame, in case
  // it has been hidden.
  if (restore_frame && !restore_bounds_.IsEmpty())
    source_tabstrip_->GetWidget()->SetBounds(restore_bounds_);

  if (detach_into_browser_ && source_tabstrip_)
    source_tabstrip_->GetWidget()->Activate();
}

void TabDragController::ResetSelection(TabStripModel* model) {
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

void TabDragController::RevertDragAt(size_t drag_index) {
  DCHECK(started_drag_);

  TabDragData* data = &(drag_data_[drag_index]);
  if (attached_tabstrip_) {
    int index =
        GetModel(attached_tabstrip_)->GetIndexOfTabContents(data->contents);
    if (attached_tabstrip_ != source_tabstrip_) {
      // The Tab was inserted into another TabStrip. We need to put it back
      // into the original one.
      GetModel(attached_tabstrip_)->DetachTabContentsAt(index);
      // TODO(beng): (Cleanup) seems like we should use Attach() for this
      //             somehow.
      GetModel(source_tabstrip_)->InsertTabContentsAt(
          data->source_model_index, data->contents,
          (data->pinned ? TabStripModel::ADD_PINNED : 0));
    } else {
      // The Tab was moved within the TabStrip where the drag was initiated.
      // Move it back to the starting location.
      GetModel(source_tabstrip_)->MoveTabContentsAt(
          index, data->source_model_index, false);
    }
  } else {
    // The Tab was detached from the TabStrip where the drag began, and has not
    // been attached to any other TabStrip. We need to put it back into the
    // source TabStrip.
    GetModel(source_tabstrip_)->InsertTabContentsAt(
        data->source_model_index, data->contents,
        (data->pinned ? TabStripModel::ADD_PINNED : 0));
  }
}

void TabDragController::CompleteDrag() {
  DCHECK(started_drag_);

  if (attached_tabstrip_) {
    attached_tabstrip_->StoppedDraggingTabs(
        GetTabsMatchingDraggedContents(attached_tabstrip_));
  } else {
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
          content::RecordAction(
              UserMetricsAction("DockingWindow_Maximize"));
          break;

        case DockInfo::LEFT_HALF:
          content::RecordAction(
              UserMetricsAction("DockingWindow_LeftHalf"));
          break;

        case DockInfo::RIGHT_HALF:
          content::RecordAction(
              UserMetricsAction("DockingWindow_RightHalf"));
          break;

        case DockInfo::BOTTOM_HALF:
          content::RecordAction(
              UserMetricsAction("DockingWindow_BottomHalf"));
          break;

        default:
          NOTREACHED();
          break;
      }
    }
    // Compel the model to construct a new window for the detached WebContents.
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

  CleanUpHiddenFrame();
}

void TabDragController::ResetDelegates() {
  DCHECK(!detach_into_browser_);
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    if (drag_data_[i].contents &&
        drag_data_[i].contents->web_contents()->GetDelegate() == this) {
      drag_data_[i].contents->web_contents()->SetDelegate(
          drag_data_[i].original_delegate);
    }
  }
}

void TabDragController::CreateDraggedView(
    const std::vector<TabRendererData>& data,
    const std::vector<gfx::Rect>& renderer_bounds) {
#if !defined(USE_AURA)
  DCHECK(!view_.get());
  DCHECK_EQ(data.size(), drag_data_.size());

  // Set up the photo booth to start capturing the contents of the dragged
  // WebContents.
  NativeViewPhotobooth* photobooth =
      NativeViewPhotobooth::Create(
          source_dragged_contents()->web_contents()->GetNativeView());

  gfx::Rect content_bounds;
  source_dragged_contents()->web_contents()->GetContainerBounds(
      &content_bounds);

  std::vector<views::View*> renderers;
  for (size_t i = 0; i < drag_data_.size(); ++i) {
    BaseTab* renderer = source_tabstrip_->CreateTabForDragging();
    renderer->SetData(data[i]);
    renderers.push_back(renderer);
  }
  // DraggedTabView takes ownership of the renderers.
  view_.reset(new DraggedTabView(renderers, renderer_bounds, mouse_offset_,
                                 content_bounds.size(), photobooth));
#else
  // Aura always hits the |detach_into_browser_| path.
  NOTREACHED();
#endif
}

gfx::Point TabDragController::GetCursorScreenPoint() const {
  // TODO(sky): see if we can convert to using Screen every where.
#if defined(OS_WIN) && !defined(USE_AURA)
  DWORD pos = GetMessagePos();
  return gfx::Point(pos);
#else
  return gfx::Screen::GetCursorScreenPoint();
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

void TabDragController::HideFrame() {
#if defined(OS_WIN) && !defined(USE_AURA)
  // We don't actually hide the window, rather we just move it way off-screen.
  // If we actually hide it, we stop receiving drag events.
  HWND frame_hwnd = source_tabstrip_->GetWidget()->GetNativeView();
  RECT wr;
  GetWindowRect(frame_hwnd, &wr);
  MoveWindow(frame_hwnd, 0xFFFF, 0xFFFF, wr.right - wr.left,
             wr.bottom - wr.top, TRUE);

  // We also save the bounds of the window prior to it being moved, so that if
  // the drag session is aborted we can restore them.
  restore_bounds_ = gfx::Rect(wr);
#else
  // Shouldn't hit as aura triggers the |detach_into_browser_| path.
  NOTREACHED();
#endif
}

void TabDragController::CleanUpHiddenFrame() {
  // If the model we started dragging from is now empty, we must ask the
  // delegate to close the frame.
  if (!detach_into_browser_ && GetModel(source_tabstrip_)->empty())
    GetModel(source_tabstrip_)->delegate()->CloseFrameAfterDragSession();
}

void TabDragController::DockDisplayerDestroyed(
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

void TabDragController::BringWindowUnderMouseToFront() {
  // If we're going to dock to another window, bring it to the front.
  gfx::NativeWindow window = dock_info_.window();
  if (!window) {
    views::View* dragged_view;
    if (view_.get())
      dragged_view = view_.get();
    else
      dragged_view = attached_tabstrip_;
    gfx::NativeView dragged_native_view =
        dragged_view->GetWidget()->GetNativeView();
    dock_windows_.insert(dragged_native_view);
    window = DockInfo::GetLocalProcessWindowAtPoint(GetCursorScreenPoint(),
                                                    dock_windows_);
    dock_windows_.erase(dragged_native_view);
  }
  if (window) {
    views::Widget* widget_window = views::Widget::GetWidgetForNativeView(
        window);
    if (widget_window)
      widget_window->StackAtTop();
    else
      return;

    // The previous call made the window appear on top of the dragged window,
    // move the dragged window to the front.
    if (view_.get())
      view_->GetWidget()->StackAtTop();
    else if (is_dragging_window_)
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

Browser* TabDragController::CreateBrowserForDrag(
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

  SetTrackedByWorkspace(browser->window()->GetNativeHandle(), false);
  browser->window()->SetBounds(new_bounds);
  return browser;
}
