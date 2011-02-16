// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/views/tabs/dragged_tab_controller.h"

#include <math.h>
#include <set>

#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "chrome/browser/browser_window.h"
#include "chrome/browser/extensions/extension_function_dispatcher.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tabs/tab_strip_model.h"
#include "chrome/browser/metrics/user_metrics.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/browser/ui/views/tabs/base_tab.h"
#include "chrome/browser/ui/views/tabs/base_tab_strip.h"
#include "chrome/browser/ui/views/tabs/browser_tab_strip_controller.h"
#include "chrome/browser/ui/views/tabs/dragged_tab_view.h"
#include "chrome/browser/ui/views/tabs/native_view_photobooth.h"
#include "chrome/browser/ui/views/tabs/side_tab.h"
#include "chrome/browser/ui/views/tabs/side_tab_strip.h"
#include "chrome/browser/ui/views/tabs/tab.h"
#include "chrome/browser/ui/views/tabs/tab_strip.h"
#include "chrome/common/notification_details.h"
#include "chrome/common/notification_source.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/animation/animation.h"
#include "ui/base/animation/animation_delegate.h"
#include "ui/base/animation/slide_animation.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "views/events/event.h"
#include "views/screen.h"
#include "views/widget/root_view.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

#if defined(OS_WIN)
#include "views/widget/widget_win.h"
#endif

#if defined(OS_LINUX)
#include <gdk/gdk.h>  // NOLINT
#include <gdk/gdkkeysyms.h>  // NOLINT
#endif

static const int kHorizontalMoveThreshold = 16;  // Pixels.

// Distance in pixels the user must move the mouse before we consider moving
// an attached vertical tab.
static const int kVerticalMoveThreshold = 8;

// If non-null there is a drag underway.
static DraggedTabController* instance_;

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
    canvas->AsCanvasSkia()->drawRoundRect(
        outer_rect, SkIntToScalar(kRoundedRectRadius),
        SkIntToScalar(kRoundedRectRadius), paint);

    ResourceBundle& rb = ResourceBundle::GetSharedInstance();

    SkBitmap* high_icon = rb.GetBitmapNamed(IDR_DOCK_HIGH);
    SkBitmap* wide_icon = rb.GetBitmapNamed(IDR_DOCK_WIDE);

    canvas->Save();
    bool rtl_ui = base::i18n::IsRTL();
    if (rtl_ui) {
      // Flip canvas to draw the mirrored tab images for RTL UI.
      canvas->TranslateInt(width(), 0);
      canvas->ScaleInt(-1, 1);
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

gfx::Point ConvertScreenPointToTabStripPoint(BaseTabStrip* tabstrip,
                                             const gfx::Point& screen_point) {
  gfx::Point tabstrip_topleft;
  views::View::ConvertPointToScreen(tabstrip, &tabstrip_topleft);
  return gfx::Point(screen_point.x() - tabstrip_topleft.x(),
                    screen_point.y() - tabstrip_topleft.y());
}

// Returns the the x-coordinate of |point| if the type of tabstrip is horizontal
// otherwise returns the y-coordinate.
int MajorAxisValue(const gfx::Point& point, BaseTabStrip* tabstrip) {
  return (tabstrip->type() == BaseTabStrip::HORIZONTAL_TAB_STRIP) ?
      point.x() : point.y();
}

}  // namespace

///////////////////////////////////////////////////////////////////////////////
// DockDisplayer

// DockDisplayer is responsible for giving the user a visual indication of a
// possible dock position (as represented by DockInfo). DockDisplayer shows
// a window with a DockView in it. Two animations are used that correspond to
// the state of DockInfo::in_enable_area.
class DraggedTabController::DockDisplayer : public ui::AnimationDelegate {
 public:
  DockDisplayer(DraggedTabController* controller,
                const DockInfo& info)
      : controller_(controller),
        popup_(NULL),
        popup_view_(NULL),
        ALLOW_THIS_IN_INITIALIZER_LIST(animation_(this)),
        hidden_(false),
        in_enable_area_(info.in_enable_area()) {
#if defined(OS_WIN)
    views::WidgetWin* popup = new views::WidgetWin;
    popup_ = popup;
    popup->set_window_style(WS_POPUP);
    popup->set_window_ex_style(WS_EX_LAYERED | WS_EX_TOOLWINDOW |
                               WS_EX_TOPMOST);
    popup->SetOpacity(0x00);
    popup->Init(NULL, info.GetPopupRect());
    popup->SetContentsView(new DockView(info.type()));
    if (info.in_enable_area())
      animation_.Reset(1);
    else
      animation_.Show();
    popup->SetWindowPos(HWND_TOP, 0, 0, 0, 0,
        SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOMOVE | SWP_SHOWWINDOW);
#else
    NOTIMPLEMENTED();
#endif
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

  // Resets the reference to the hosting DraggedTabController. This is invoked
  // when the DraggedTabController is destoryed.
  void clear_controller() { controller_ = NULL; }

  // NativeView of the window we create.
  gfx::NativeView popup_view() { return popup_view_; }

  // Starts the hide animation. When the window is closed the
  // DraggedTabController is notified by way of the DockDisplayerDestroyed
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
#if defined(OS_WIN)
    static_cast<views::WidgetWin*>(popup_)->Close();
#else
    NOTIMPLEMENTED();
#endif
    delete this;
  }

  virtual void UpdateLayeredAlpha() {
#if defined(OS_WIN)
    double scale = in_enable_area_ ? 1 : .5;
    static_cast<views::WidgetWin*>(popup_)->SetOpacity(
        static_cast<BYTE>(animation_.GetCurrentValue() * scale * 255.0));
    popup_->GetRootView()->SchedulePaint();
#else
    NOTIMPLEMENTED();
#endif
  }

 private:
  // DraggedTabController that created us.
  DraggedTabController* controller_;

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

///////////////////////////////////////////////////////////////////////////////
// DraggedTabController, public:

DraggedTabController::DraggedTabController(BaseTab* source_tab,
                                           BaseTabStrip* source_tabstrip)
    : dragged_contents_(NULL),
      original_delegate_(NULL),
      source_tabstrip_(source_tabstrip),
      source_model_index_(source_tabstrip->GetModelIndexOfBaseTab(source_tab)),
      attached_tabstrip_(NULL),
      attached_tab_(NULL),
      offset_to_width_ratio_(0),
      old_focused_view_(NULL),
      last_move_screen_loc_(0),
      mini_(source_tab->data().mini),
      pinned_(source_tabstrip->IsTabPinned(source_tab)),
      started_drag_(false),
      active_(true) {
  instance_ = this;
  SetDraggedContents(
      GetModel(source_tabstrip_)->GetTabContentsAt(source_model_index_));
  // Listen for Esc key presses.
  MessageLoopForUI::current()->AddObserver(this);
}

DraggedTabController::~DraggedTabController() {
  if (instance_ == this)
    instance_ = NULL;

  MessageLoopForUI::current()->RemoveObserver(this);
  // Need to delete the view here manually _before_ we reset the dragged
  // contents to NULL, otherwise if the view is animating to its destination
  // bounds, it won't be able to clean up properly since its cleanup routine
  // uses GetIndexForDraggedContents, which will be invalid.
  view_.reset(NULL);
  SetDraggedContents(NULL);  // This removes our observer.
}

// static
bool DraggedTabController::IsAttachedTo(BaseTabStrip* tab_strip) {
  return instance_ && instance_->active_ &&
      instance_->attached_tabstrip_ == tab_strip;
}

void DraggedTabController::CaptureDragInfo(views::View* tab,
                                           const gfx::Point& mouse_offset) {
  if (tab->width() > 0) {
    offset_to_width_ratio_ = static_cast<float>(mouse_offset.x()) /
        static_cast<float>(tab->width());
  }
  start_screen_point_ = GetCursorScreenPoint();
  mouse_offset_ = mouse_offset;
  InitWindowCreatePoint();
}

void DraggedTabController::Drag() {
  bring_to_front_timer_.Stop();

  // Before we get to dragging anywhere, ensure that we consider ourselves
  // attached to the source tabstrip.
  if (!started_drag_ && CanStartDrag()) {
    started_drag_ = true;
    SaveFocus();
    Attach(source_tabstrip_, gfx::Point());
  }

  if (started_drag_)
    ContinueDragging();
}

void DraggedTabController::EndDrag(bool canceled) {
  EndDragImpl(canceled ? CANCELED : NORMAL);
}

///////////////////////////////////////////////////////////////////////////////
// DraggedTabController, PageNavigator implementation:

void DraggedTabController::OpenURLFromTab(TabContents* source,
                                          const GURL& url,
                                          const GURL& referrer,
                                          WindowOpenDisposition disposition,
                                          PageTransition::Type transition) {
  if (original_delegate_) {
    if (disposition == CURRENT_TAB)
      disposition = NEW_WINDOW;

    original_delegate_->OpenURLFromTab(source, url, referrer,
                                       disposition, transition);
  }
}

///////////////////////////////////////////////////////////////////////////////
// DraggedTabController, TabContentsDelegate implementation:

void DraggedTabController::NavigationStateChanged(const TabContents* source,
                                                  unsigned changed_flags) {
  if (view_.get())
    view_->Update();
}

void DraggedTabController::AddNewContents(TabContents* source,
                                          TabContents* new_contents,
                                          WindowOpenDisposition disposition,
                                          const gfx::Rect& initial_pos,
                                          bool user_gesture) {
  DCHECK(disposition != CURRENT_TAB);

  // Theoretically could be called while dragging if the page tries to
  // spawn a window. Route this message back to the browser in most cases.
  if (original_delegate_) {
    original_delegate_->AddNewContents(source, new_contents, disposition,
                                       initial_pos, user_gesture);
  }
}

void DraggedTabController::ActivateContents(TabContents* contents) {
  // Ignored.
}

void DraggedTabController::DeactivateContents(TabContents* contents) {
  // Ignored.
}

void DraggedTabController::LoadingStateChanged(TabContents* source) {
  // It would be nice to respond to this message by changing the
  // screen shot in the dragged tab.
  if (view_.get())
    view_->Update();
}

void DraggedTabController::CloseContents(TabContents* source) {
  // Theoretically could be called by a window. Should be ignored
  // because window.close() is ignored (usually, even though this
  // method gets called.)
}

void DraggedTabController::MoveContents(TabContents* source,
                                        const gfx::Rect& pos) {
  // Theoretically could be called by a web page trying to move its
  // own window. Should be ignored since we're moving the window...
}

void DraggedTabController::ToolbarSizeChanged(TabContents* source,
                                            bool finished) {
  // Dragged tabs don't care about this.
}

void DraggedTabController::UpdateTargetURL(TabContents* source,
                                           const GURL& url) {
  // Ignored.
}

///////////////////////////////////////////////////////////////////////////////
// DraggedTabController, NotificationObserver implementation:

void DraggedTabController::Observe(NotificationType type,
                                   const NotificationSource& source,
                                   const NotificationDetails& details) {
  DCHECK(type == NotificationType::TAB_CONTENTS_DESTROYED);
  DCHECK(Source<TabContents>(source).ptr() ==
         dragged_contents_->tab_contents());
  EndDragImpl(TAB_DESTROYED);
}

///////////////////////////////////////////////////////////////////////////////
// DraggedTabController, MessageLoop::Observer implementation:

#if defined(OS_WIN)
void DraggedTabController::WillProcessMessage(const MSG& msg) {
}

void DraggedTabController::DidProcessMessage(const MSG& msg) {
  // If the user presses ESC during a drag, we need to abort and revert things
  // to the way they were. This is the most reliable way to do this since no
  // single view or window reliably receives events throughout all the various
  // kinds of tab dragging.
  if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
    EndDrag(true);
}
#else
void DraggedTabController::WillProcessEvent(GdkEvent* event) {
}

void DraggedTabController::DidProcessEvent(GdkEvent* event) {
  if (event->type == GDK_KEY_PRESS &&
      reinterpret_cast<GdkEventKey*>(event)->keyval == GDK_Escape) {
    EndDrag(true);
  }
}
#endif

///////////////////////////////////////////////////////////////////////////////
// DraggedTabController, private:

void DraggedTabController::InitWindowCreatePoint() {
  // window_create_point_ is only used in CompleteDrag() (through
  // GetWindowCreatePoint() to get the start point of the docked window) when
  // the attached_tabstrip_ is NULL and all the window's related bound
  // information are obtained from source_tabstrip_. So, we need to get the
  // first_tab based on source_tabstrip_, not attached_tabstrip_. Otherwise,
  // the window_create_point_ is not in the correct coordinate system. Please
  // refer to http://crbug.com/6223 comment #15 for detailed information.
  views::View* first_tab = source_tabstrip_->base_tab_at_tab_index(0);
  views::View::ConvertPointToWidget(first_tab, &first_source_tab_point_);
  UpdateWindowCreatePoint();
}

void DraggedTabController::UpdateWindowCreatePoint() {
  // See comments in InitWindowCreatePoint for details on this.
  window_create_point_ = first_source_tab_point_;
  window_create_point_.Offset(mouse_offset_.x(), mouse_offset_.y());
}

gfx::Point DraggedTabController::GetWindowCreatePoint() const {
  gfx::Point cursor_point = GetCursorScreenPoint();
  if (dock_info_.type() != DockInfo::NONE && dock_info_.in_enable_area()) {
    // If we're going to dock, we need to return the exact coordinate,
    // otherwise we may attempt to maximize on the wrong monitor.
    return cursor_point;
  }
  // If the cursor is outside the monitor area, move it inside. For example,
  // dropping a tab onto the task bar on Windows produces this situation.
  gfx::Rect work_area = views::Screen::GetMonitorWorkAreaNearestPoint(
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

void DraggedTabController::UpdateDockInfo(const gfx::Point& screen_point) {
  // Update the DockInfo for the current mouse coordinates.
  DockInfo dock_info = GetDockInfoAtPoint(screen_point);
  if (source_tabstrip_->type() == BaseTabStrip::VERTICAL_TAB_STRIP &&
      ((dock_info.type() == DockInfo::LEFT_OF_WINDOW &&
        !base::i18n::IsRTL()) ||
       (dock_info.type() == DockInfo::RIGHT_OF_WINDOW &&
        base::i18n::IsRTL()))) {
    // For side tabs it's way to easy to trigger to docking along the left/right
    // edge, so we disable it.
    dock_info = DockInfo();
  }
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

void DraggedTabController::SetDraggedContents(
    TabContentsWrapper* new_contents) {
  if (dragged_contents_) {
    registrar_.Remove(this,
                      NotificationType::TAB_CONTENTS_DESTROYED,
                      Source<TabContents>(dragged_contents_->tab_contents()));
    if (original_delegate_)
      dragged_contents_->tab_contents()->set_delegate(original_delegate_);
  }
  original_delegate_ = NULL;
  dragged_contents_ = new_contents;
  if (dragged_contents_) {
    registrar_.Add(this,
                   NotificationType::TAB_CONTENTS_DESTROYED,
                   Source<TabContents>(dragged_contents_->tab_contents()));

    // We need to be the delegate so we receive messages about stuff,
    // otherwise our dragged_contents() may be replaced and subsequently
    // collected/destroyed while the drag is in process, leading to
    // nasty crashes.
    original_delegate_ = dragged_contents_->tab_contents()->delegate();
    dragged_contents_->tab_contents()->set_delegate(this);
  }
}

void DraggedTabController::SaveFocus() {
  if (!old_focused_view_) {
    old_focused_view_ = source_tabstrip_->GetRootView()->GetFocusedView();
    source_tabstrip_->GetRootView()->FocusView(source_tabstrip_);
  }
}

void DraggedTabController::RestoreFocus() {
  if (old_focused_view_ && attached_tabstrip_ == source_tabstrip_)
    old_focused_view_->GetRootView()->FocusView(old_focused_view_);
  old_focused_view_ = NULL;
}

bool DraggedTabController::CanStartDrag() const {
  // Determine if the mouse has moved beyond a minimum elasticity distance in
  // any direction from the starting point.
  static const int kMinimumDragDistance = 10;
  gfx::Point screen_point = GetCursorScreenPoint();
  int x_offset = abs(screen_point.x() - start_screen_point_.x());
  int y_offset = abs(screen_point.y() - start_screen_point_.y());
  return sqrt(pow(static_cast<float>(x_offset), 2) +
              pow(static_cast<float>(y_offset), 2)) > kMinimumDragDistance;
}

void DraggedTabController::ContinueDragging() {
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

#if defined(OS_CHROMEOS) || defined(TOUCH_UI)
  // We don't allow detaching in chrome os.
  BaseTabStrip* target_tabstrip = source_tabstrip_;
#else
  // Determine whether or not we have dragged over a compatible TabStrip in
  // another browser window. If we have, we should attach to it and start
  // dragging within it.
  BaseTabStrip* target_tabstrip = GetTabStripForPoint(screen_point);
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
    bring_to_front_timer_.Start(
        base::TimeDelta::FromMilliseconds(kBringToFrontDelay), this,
        &DraggedTabController::BringWindowUnderMouseToFront);
  }

  UpdateDockInfo(screen_point);

  if (attached_tabstrip_)
    MoveAttachedTab(screen_point);
  else
    MoveDetachedTab(screen_point);
}

void DraggedTabController::MoveAttachedTab(const gfx::Point& screen_point) {
  DCHECK(attached_tabstrip_);
  DCHECK(!view_.get());

  gfx::Point dragged_view_point = GetAttachedTabDragPoint(screen_point);
  TabStripModel* attached_model = GetModel(attached_tabstrip_);
  int from_index = attached_model->GetIndexOfTabContents(dragged_contents_);

  int threshold = kVerticalMoveThreshold;
  if (attached_tabstrip_->type() == BaseTabStrip::HORIZONTAL_TAB_STRIP) {
    TabStrip* tab_strip = static_cast<TabStrip*>(attached_tabstrip_);

    // Determine the horizontal move threshold. This is dependent on the width
    // of tabs. The smaller the tabs compared to the standard size, the smaller
    // the threshold.
    double unselected, selected;
    tab_strip->GetCurrentTabWidths(&unselected, &selected);
    double ratio = unselected / Tab::GetStandardSize().width();
    threshold = static_cast<int>(ratio * kHorizontalMoveThreshold);
  }

  // Update the model, moving the TabContents from one index to another. Do this
  // only if we have moved a minimum distance since the last reorder (to prevent
  // jitter).
  if (abs(MajorAxisValue(screen_point, attached_tabstrip_) -
          last_move_screen_loc_) > threshold) {
    gfx::Point dragged_view_screen_point(dragged_view_point);
    views::View::ConvertPointToScreen(attached_tabstrip_,
                                      &dragged_view_screen_point);
    gfx::Rect bounds = GetDraggedViewTabStripBounds(dragged_view_screen_point);
    int to_index = GetInsertionIndexForDraggedBounds(bounds, true);
    to_index = NormalizeIndexToAttachedTabStrip(to_index);
    if (from_index != to_index) {
      last_move_screen_loc_ = MajorAxisValue(screen_point, attached_tabstrip_);
      attached_model->MoveTabContentsAt(from_index, to_index, true);
    }
  }

  attached_tab_->SchedulePaint();
  attached_tab_->SetX(dragged_view_point.x());
  attached_tab_->SetX(
      attached_tabstrip_->GetMirroredXForRect(attached_tab_->bounds()));
  attached_tab_->SetY(dragged_view_point.y());
  attached_tab_->SchedulePaint();
}

void DraggedTabController::MoveDetachedTab(const gfx::Point& screen_point) {
  DCHECK(!attached_tabstrip_);
  DCHECK(view_.get());

  int x = screen_point.x() - mouse_offset_.x();
  int y = screen_point.y() - mouse_offset_.y();

  // Move the View. There are no changes to the model if we're detached.
  view_->MoveTo(gfx::Point(x, y));
}

DockInfo DraggedTabController::GetDockInfoAtPoint(
    const gfx::Point& screen_point) {
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

  gfx::NativeView dragged_hwnd = view_->GetWidget()->GetNativeView();
  dock_windows_.insert(dragged_hwnd);
  DockInfo info = DockInfo::GetDockInfoAtPoint(screen_point, dock_windows_);
  dock_windows_.erase(dragged_hwnd);
  return info;
}

BaseTabStrip* DraggedTabController::GetTabStripForPoint(
    const gfx::Point& screen_point) {
  gfx::NativeView dragged_view = NULL;
  if (view_.get()) {
    dragged_view = view_->GetWidget()->GetNativeView();
    dock_windows_.insert(dragged_view);
  }
  gfx::NativeWindow local_window =
      DockInfo::GetLocalProcessWindowAtPoint(screen_point, dock_windows_);
  if (dragged_view)
    dock_windows_.erase(dragged_view);
  if (!local_window)
    return NULL;
  BrowserView* browser =
      BrowserView::GetBrowserViewForNativeWindow(local_window);
  // We don't allow drops on windows that don't have tabstrips.
  if (!browser ||
      !browser->browser()->SupportsWindowFeature(Browser::FEATURE_TABSTRIP))
    return NULL;

  BaseTabStrip* other_tabstrip = browser->tabstrip();
  if (!other_tabstrip->controller()->IsCompatibleWith(source_tabstrip_))
    return NULL;
  return GetTabStripIfItContains(other_tabstrip, screen_point);
}

BaseTabStrip* DraggedTabController::GetTabStripIfItContains(
    BaseTabStrip* tabstrip, const gfx::Point& screen_point) const {
  static const int kVerticalDetachMagnetism = 15;
  static const int kHorizontalDetachMagnetism = 15;
  // Make sure the specified screen point is actually within the bounds of the
  // specified tabstrip...
  gfx::Rect tabstrip_bounds = GetViewScreenBounds(tabstrip);
  if (tabstrip->type() == BaseTabStrip::HORIZONTAL_TAB_STRIP) {
    if (screen_point.x() < tabstrip_bounds.right() &&
        screen_point.x() >= tabstrip_bounds.x()) {
      // TODO(beng): make this be relative to the start position of the mouse
      // for the source TabStrip.
      int upper_threshold = tabstrip_bounds.bottom() + kVerticalDetachMagnetism;
      int lower_threshold = tabstrip_bounds.y() - kVerticalDetachMagnetism;
      if (screen_point.y() >= lower_threshold &&
          screen_point.y() <= upper_threshold) {
        return tabstrip;
      }
    }
  } else {
    if (screen_point.y() < tabstrip_bounds.bottom() &&
        screen_point.y() >= tabstrip_bounds.y()) {
      int upper_threshold = tabstrip_bounds.right() +
          kHorizontalDetachMagnetism;
      int lower_threshold = tabstrip_bounds.x() - kHorizontalDetachMagnetism;
      if (screen_point.x() >= lower_threshold &&
          screen_point.x() <= upper_threshold) {
        return tabstrip;
      }
    }
  }
  return NULL;
}

void DraggedTabController::Attach(BaseTabStrip* attached_tabstrip,
                                  const gfx::Point& screen_point) {
  DCHECK(!attached_tabstrip_);  // We should already have detached by the time
                                // we get here.
  DCHECK(!attached_tab_);  // Similarly there should be no attached tab.

  attached_tabstrip_ = attached_tabstrip;

  // We don't need the photo-booth while we're attached.
  photobooth_.reset(NULL);

  // And we don't need the dragged view.
  view_.reset();

  BaseTab* tab = GetTabMatchingDraggedContents(attached_tabstrip_);

  if (!tab) {
    // There is no Tab in |attached_tabstrip| that corresponds to the dragged
    // TabContents. We must now create one.

    // Remove ourselves as the delegate now that the dragged TabContents is
    // being inserted back into a Browser.
    dragged_contents_->tab_contents()->set_delegate(NULL);
    original_delegate_ = NULL;

    // Return the TabContents' to normalcy.
    dragged_contents_->tab_contents()->set_capturing_contents(false);

    // Inserting counts as a move. We don't want the tabs to jitter when the
    // user moves the tab immediately after attaching it.
    last_move_screen_loc_ = MajorAxisValue(screen_point, attached_tabstrip);

    // Figure out where to insert the tab based on the bounds of the dragged
    // representation and the ideal bounds of the other Tabs already in the
    // strip. ("ideal bounds" are stable even if the Tabs' actual bounds are
    // changing due to animation).
    gfx::Rect bounds = GetDraggedViewTabStripBounds(screen_point);
    int index = GetInsertionIndexForDraggedBounds(bounds, false);
    attached_tabstrip_->set_attaching_dragged_tab(true);
    GetModel(attached_tabstrip_)->InsertTabContentsAt(
        index, dragged_contents_,
        TabStripModel::ADD_SELECTED |
            (pinned_ ? TabStripModel::ADD_PINNED : 0));
    attached_tabstrip_->set_attaching_dragged_tab(false);

    tab = GetTabMatchingDraggedContents(attached_tabstrip_);
  }
  DCHECK(tab);  // We should now have a tab.
  attached_tab_ = tab;
  attached_tabstrip_->StartedDraggingTab(tab);

  if (attached_tabstrip_->type() == BaseTabStrip::HORIZONTAL_TAB_STRIP) {
    // The size of the dragged tab may have changed. Adjust the x offset so that
    // ratio of mouse_offset_ to original width is maintained.
    mouse_offset_.set_x(static_cast<int>(offset_to_width_ratio_ *
                                         static_cast<int>(tab->width())));
  }

  // Move the corresponding window to the front.
  attached_tabstrip_->GetWindow()->Activate();
}

void DraggedTabController::Detach() {
  // Prevent the TabContents' HWND from being hidden by any of the model
  // operations performed during the drag.
  dragged_contents_->tab_contents()->set_capturing_contents(true);

  // Update the Model.
  TabRendererData tab_data = attached_tab_->data();
  TabStripModel* attached_model = GetModel(attached_tabstrip_);
  int index = attached_model->GetIndexOfTabContents(dragged_contents_);
  DCHECK(index != -1);
  // Hide the tab so that the user doesn't see it animate closed.
  attached_tab_->SetVisible(false);
  int attached_tab_width = attached_tab_->width();
  attached_model->DetachTabContentsAt(index);
  // Detaching may end up deleting the tab, drop references to it.
  attached_tab_ = NULL;

  // If we've removed the last Tab from the TabStrip, hide the frame now.
  if (attached_model->empty())
    HideFrame();

  // Set up the photo booth to start capturing the contents of the dragged
  // TabContents.
  if (!photobooth_.get()) {
    photobooth_.reset(NativeViewPhotobooth::Create(
        dragged_contents_->tab_contents()->GetNativeView()));
  }

  // Create the dragged view.
  EnsureDraggedView(tab_data);
  view_->SetTabWidthAndUpdate(attached_tab_width, photobooth_.get());

  // Detaching resets the delegate, but we still want to be the delegate.
  dragged_contents_->tab_contents()->set_delegate(this);

  attached_tabstrip_ = NULL;
}

int DraggedTabController::GetInsertionIndexForDraggedBounds(
    const gfx::Rect& dragged_bounds,
    bool is_tab_attached) const {
  int right_tab_x = 0;
  int bottom_tab_y = 0;

  // If the UI layout of the tab strip is right-to-left, we need to mirror the
  // bounds of the dragged tab before performing the drag/drop related
  // calculations. We mirror the dragged bounds because we determine the
  // position of each tab on the tab strip by calling GetBounds() (without the
  // mirroring transformation flag) which effectively means that even though
  // the tabs are rendered from right to left, the code performs the
  // calculation as if the tabs are laid out from left to right. Mirroring the
  // dragged bounds adjusts the coordinates of the tab we are dragging so that
  // it uses the same orientation used by the tabs on the tab strip.
  gfx::Rect adjusted_bounds(dragged_bounds);
  adjusted_bounds.set_x(
      attached_tabstrip_->GetMirroredXForRect(adjusted_bounds));

  int index = -1;
  for (int i = 0; i < attached_tabstrip_->tab_count(); ++i) {
    const gfx::Rect& ideal_bounds = attached_tabstrip_->ideal_bounds(i);
    if (attached_tabstrip_->type() == BaseTabStrip::HORIZONTAL_TAB_STRIP) {
      gfx::Rect left_half = ideal_bounds;
      left_half.set_width(left_half.width() / 2);
      gfx::Rect right_half = ideal_bounds;
      right_half.set_width(ideal_bounds.width() - left_half.width());
      right_half.set_x(left_half.right());
      right_tab_x = right_half.right();
      if (adjusted_bounds.x() >= right_half.x() &&
          adjusted_bounds.x() < right_half.right()) {
        index = i + 1;
        break;
      } else if (adjusted_bounds.x() >= left_half.x() &&
                 adjusted_bounds.x() < left_half.right()) {
        index = i;
        break;
      }
    } else {
      // Vertical tab strip.
      int max_y = ideal_bounds.bottom();
      int mid_y = ideal_bounds.y() + ideal_bounds.height() / 2;
      bottom_tab_y = max_y;
      if (adjusted_bounds.y() < mid_y) {
        index = i;
        break;
      } else if (adjusted_bounds.y() >= mid_y && adjusted_bounds.y() < max_y) {
        index = i + 1;
        break;
      }
    }
  }
  if (index == -1) {
    if ((attached_tabstrip_->type() == BaseTabStrip::HORIZONTAL_TAB_STRIP &&
         adjusted_bounds.right() > right_tab_x) ||
        (attached_tabstrip_->type() == BaseTabStrip::VERTICAL_TAB_STRIP &&
         adjusted_bounds.y() >= bottom_tab_y)) {
      index = GetModel(attached_tabstrip_)->count();
    } else {
      index = 0;
    }
  }

  index = GetModel(attached_tabstrip_)->ConstrainInsertionIndex(index, mini_);
  if (is_tab_attached && mini_ &&
      index == GetModel(attached_tabstrip_)->IndexOfFirstNonMiniTab()) {
    index--;
  }
  return index;
}

gfx::Rect DraggedTabController::GetDraggedViewTabStripBounds(
    const gfx::Point& screen_point) {
  gfx::Point client_point =
      ConvertScreenPointToTabStripPoint(attached_tabstrip_, screen_point);
  // attached_tab_ is NULL when inserting into a new tabstrip.
  if (attached_tab_) {
    return gfx::Rect(client_point.x(), client_point.y(),
                     attached_tab_->width(), attached_tab_->height());
  }

  if (attached_tabstrip_->type() == BaseTabStrip::HORIZONTAL_TAB_STRIP) {
    double sel_width, unselected_width;
    static_cast<TabStrip*>(attached_tabstrip_)->GetCurrentTabWidths(
        &sel_width, &unselected_width);
    return gfx::Rect(client_point.x(), client_point.y(),
                     static_cast<int>(sel_width),
                     Tab::GetStandardSize().height());
  }

  return gfx::Rect(client_point.x(), client_point.y(),
                   attached_tabstrip_->width(),
                   SideTab::GetPreferredHeight());
}

gfx::Point DraggedTabController::GetAttachedTabDragPoint(
    const gfx::Point& screen_point) {
  DCHECK(attached_tabstrip_);  // The tab must be attached.

  int x = screen_point.x() - mouse_offset_.x();
  int y = screen_point.y() - mouse_offset_.y();

  gfx::Point tab_loc(x, y);
  views::View::ConvertPointToView(NULL, attached_tabstrip_, &tab_loc);

  x = tab_loc.x();
  y = tab_loc.y();

  const gfx::Size& tab_size = attached_tab_->size();

  if (attached_tabstrip_->type() == BaseTabStrip::HORIZONTAL_TAB_STRIP) {
    int max_x = attached_tabstrip_->width() - tab_size.width();
    x = std::min(std::max(x, 0), max_x);
    y = 0;
  } else {
    x = SideTabStrip::kTabStripInset;
    int max_y = attached_tabstrip_->height() - tab_size.height();
    y = std::min(std::max(y, SideTabStrip::kTabStripInset), max_y);
  }
  return gfx::Point(x, y);
}

BaseTab* DraggedTabController::GetTabMatchingDraggedContents(
    BaseTabStrip* tabstrip) const {
  int model_index =
      GetModel(tabstrip)->GetIndexOfTabContents(dragged_contents_);
  return model_index == TabStripModel::kNoTab ?
      NULL : tabstrip->GetBaseTabAtModelIndex(model_index);
}

void DraggedTabController::EndDragImpl(EndDragType type) {
  // WARNING: this may be invoked multiple times. In particular, if deletion
  // occurs after a delay (as it does when the tab is released in the original
  // tab strip) and the navigation controller/tab contents is deleted before
  // the animation finishes, this is invoked twice. The second time through
  // type == TAB_DESTROYED.

  active_ = false;

  bring_to_front_timer_.Stop();

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
    if (dragged_contents_ &&
        dragged_contents_->tab_contents()->delegate() == this)
      dragged_contents_->tab_contents()->set_delegate(original_delegate_);
  } else {
    // If we get here it means the NavigationController is going down. Don't
    // attempt to do any cleanup other than resetting the delegate (if we're
    // still the delegate).
    if (dragged_contents_ &&
        dragged_contents_->tab_contents()->delegate() == this)
      dragged_contents_->tab_contents()->set_delegate(NULL);
    dragged_contents_ = NULL;
  }

  // The delegate of the dragged contents should have been reset. Unset the
  // original delegate so that we don't attempt to reset the delegate when
  // deleted.
  DCHECK(!dragged_contents_ ||
         dragged_contents_->tab_contents()->delegate() != this);
  original_delegate_ = NULL;

  source_tabstrip_->DestroyDragController();
}

void DraggedTabController::RevertDrag() {
  DCHECK(started_drag_);

  // We save this here because code below will modify |attached_tabstrip_|.
  bool restore_frame = attached_tabstrip_ != source_tabstrip_;
  if (attached_tabstrip_) {
    int index = GetModel(attached_tabstrip_)->GetIndexOfTabContents(
        dragged_contents_);
    if (attached_tabstrip_ != source_tabstrip_) {
      // The Tab was inserted into another TabStrip. We need to put it back
      // into the original one.
      GetModel(attached_tabstrip_)->DetachTabContentsAt(index);
      // TODO(beng): (Cleanup) seems like we should use Attach() for this
      //             somehow.
      attached_tabstrip_ = source_tabstrip_;
      GetModel(source_tabstrip_)->InsertTabContentsAt(
          source_model_index_, dragged_contents_,
          TabStripModel::ADD_SELECTED |
              (pinned_ ? TabStripModel::ADD_PINNED : 0));
    } else {
      // The Tab was moved within the TabStrip where the drag was initiated.
      // Move it back to the starting location.
      GetModel(source_tabstrip_)->MoveTabContentsAt(index, source_model_index_,
          true);
      source_tabstrip_->StoppedDraggingTab(attached_tab_);
    }
  } else {
    // TODO(beng): (Cleanup) seems like we should use Attach() for this
    //             somehow.
    attached_tabstrip_ = source_tabstrip_;
    // The Tab was detached from the TabStrip where the drag began, and has not
    // been attached to any other TabStrip. We need to put it back into the
    // source TabStrip.
    GetModel(source_tabstrip_)->InsertTabContentsAt(
        source_model_index_, dragged_contents_,
        TabStripModel::ADD_SELECTED |
            (pinned_ ? TabStripModel::ADD_PINNED : 0));
  }

  // If we're not attached to any TabStrip, or attached to some other TabStrip,
  // we need to restore the bounds of the original TabStrip's frame, in case
  // it has been hidden.
  if (restore_frame) {
    if (!restore_bounds_.IsEmpty()) {
#if defined(OS_WIN)
      HWND frame_hwnd = source_tabstrip_->GetWidget()->GetNativeView();
      MoveWindow(frame_hwnd, restore_bounds_.x(), restore_bounds_.y(),
                 restore_bounds_.width(), restore_bounds_.height(), TRUE);
#else
      NOTIMPLEMENTED();
#endif
    }
  }
}

void DraggedTabController::CompleteDrag() {
  DCHECK(started_drag_);

  if (attached_tabstrip_) {
    attached_tabstrip_->StoppedDraggingTab(attached_tab_);
  } else {
    if (dock_info_.type() != DockInfo::NONE) {
      Profile* profile = GetModel(source_tabstrip_)->profile();
      switch (dock_info_.type()) {
        case DockInfo::LEFT_OF_WINDOW:
          UserMetrics::RecordAction(UserMetricsAction("DockingWindow_Left"),
                                    profile);
          break;

        case DockInfo::RIGHT_OF_WINDOW:
          UserMetrics::RecordAction(UserMetricsAction("DockingWindow_Right"),
                                    profile);
          break;

        case DockInfo::BOTTOM_OF_WINDOW:
          UserMetrics::RecordAction(UserMetricsAction("DockingWindow_Bottom"),
                                    profile);
          break;

        case DockInfo::TOP_OF_WINDOW:
          UserMetrics::RecordAction(UserMetricsAction("DockingWindow_Top"),
                                    profile);
          break;

        case DockInfo::MAXIMIZE:
          UserMetrics::RecordAction(UserMetricsAction("DockingWindow_Maximize"),
                                    profile);
          break;

        case DockInfo::LEFT_HALF:
          UserMetrics::RecordAction(UserMetricsAction("DockingWindow_LeftHalf"),
                                    profile);
          break;

        case DockInfo::RIGHT_HALF:
          UserMetrics::RecordAction(
              UserMetricsAction("DockingWindow_RightHalf"),
              profile);
          break;

        case DockInfo::BOTTOM_HALF:
          UserMetrics::RecordAction(
              UserMetricsAction("DockingWindow_BottomHalf"),
              profile);
          break;

        default:
          NOTREACHED();
          break;
      }
    }
    // Compel the model to construct a new window for the detached TabContents.
    views::Window* window = source_tabstrip_->GetWindow();
    gfx::Rect window_bounds(window->GetNormalBounds());
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
        dragged_contents_, window_bounds, dock_info_, window->IsMaximized());
    TabStripModel* new_model = new_browser->tabstrip_model();
    new_model->SetTabPinned(new_model->GetIndexOfTabContents(dragged_contents_),
                            pinned_);
    new_browser->window()->Show();
  }

  CleanUpHiddenFrame();
}

void DraggedTabController::EnsureDraggedView(const TabRendererData& data) {
  if (!view_.get()) {
    gfx::Rect tab_bounds;
    dragged_contents_->tab_contents()->GetContainerBounds(&tab_bounds);
    BaseTab* renderer = source_tabstrip_->CreateTabForDragging();
    renderer->SetData(data);
    // DraggedTabView takes ownership of renderer.
    view_.reset(new DraggedTabView(renderer, mouse_offset_,
                                   tab_bounds.size(),
                                   Tab::GetMinimumSelectedSize()));
  }
}

gfx::Point DraggedTabController::GetCursorScreenPoint() const {
#if defined(OS_WIN)
  DWORD pos = GetMessagePos();
  return gfx::Point(pos);
#else
  gint x, y;
  gdk_display_get_pointer(gdk_display_get_default(), NULL, &x, &y, NULL);
  return gfx::Point(x, y);
#endif
}

gfx::Rect DraggedTabController::GetViewScreenBounds(views::View* view) const {
  gfx::Point view_topleft;
  views::View::ConvertPointToScreen(view, &view_topleft);
  gfx::Rect view_screen_bounds = view->GetLocalBounds();
  view_screen_bounds.Offset(view_topleft.x(), view_topleft.y());
  return view_screen_bounds;
}

int DraggedTabController::NormalizeIndexToAttachedTabStrip(int index) const {
  DCHECK(attached_tabstrip_) << "Can only be called when attached!";
  TabStripModel* attached_model = GetModel(attached_tabstrip_);
  if (index >= attached_model->count())
    return attached_model->count() - 1;
  if (index == TabStripModel::kNoTab)
    return 0;
  return index;
}

void DraggedTabController::HideFrame() {
#if defined(OS_WIN)
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
  NOTIMPLEMENTED();
#endif
}

void DraggedTabController::CleanUpHiddenFrame() {
  // If the model we started dragging from is now empty, we must ask the
  // delegate to close the frame.
  if (GetModel(source_tabstrip_)->empty())
    GetModel(source_tabstrip_)->delegate()->CloseFrameAfterDragSession();
}

void DraggedTabController::DockDisplayerDestroyed(
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

void DraggedTabController::BringWindowUnderMouseToFront() {
  // If we're going to dock to another window, bring it to the front.
  gfx::NativeWindow window = dock_info_.window();
  if (!window) {
    gfx::NativeView dragged_view = view_->GetWidget()->GetNativeView();
    dock_windows_.insert(dragged_view);
    window = DockInfo::GetLocalProcessWindowAtPoint(GetCursorScreenPoint(),
                                                    dock_windows_);
    dock_windows_.erase(dragged_view);
  }
  if (window) {
#if defined(OS_WIN)
    // Move the window to the front.
    SetWindowPos(window, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);

    // The previous call made the window appear on top of the dragged window,
    // move the dragged window to the front.
    SetWindowPos(view_->GetWidget()->GetNativeView(), HWND_TOP, 0, 0, 0, 0,
                 SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOSIZE);
#else
    NOTIMPLEMENTED();
#endif
  }
}

TabStripModel* DraggedTabController::GetModel(BaseTabStrip* tabstrip) const {
  return static_cast<BrowserTabStripController*>(tabstrip->controller())->
      model();
}
