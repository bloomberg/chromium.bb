// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Draws the view for the balloons.

#include "chrome/browser/chromeos/notifications/notification_panel.h"

#include <algorithm>

#include "base/bind.h"
#include "chrome/browser/chromeos/notifications/balloon_collection_impl.h"
#include "chrome/browser/chromeos/notifications/balloon_view.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_details.h"
#include "content/public/browser/notification_source.h"
#include "grit/generated_resources.h"
#include "third_party/cros_system_api/window_manager/chromeos_wm_ipc_enums.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas.h"
#include "ui/views/background.h"
#include "ui/views/controls/native/native_view_host.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/widget/native_widget_gtk.h"

#define SET_STATE(state) SetState(state, __PRETTY_FUNCTION__)

using views::Widget;

namespace {
// Minimum and maximum size of balloon content.
const int kBalloonMinWidth = 300;
const int kBalloonMaxWidth = 300;
const int kBalloonMinHeight = 24;
const int kBalloonMaxHeight = 120;

// Maximum height of the notification panel.
// TODO(oshima): Get this from system's metrics.
const int kMaxPanelHeight = 400;

// The duration for a new notification to become stale.
const int kStaleTimeoutInSeconds = 10;

// Panel Background color
const SkColor kPanelBackground = 0xFFEEEEEE;

using chromeos::BalloonViewImpl;
using chromeos::NotificationPanel;

#if !defined(NDEBUG)
// A utility function to convert State enum to string.
const char* ToStr(const NotificationPanel::State state) {
  switch (state) {
    case NotificationPanel::FULL:
      return "full";
    case NotificationPanel::KEEP_SIZE:
      return "keep_size";
    case NotificationPanel::STICKY_AND_NEW:
      return "sticky_new";
    case NotificationPanel::MINIMIZED:
      return "minimized";
    case NotificationPanel::CLOSED:
      return "closed";
    default:
      return "unknown";
  }
}
#endif

chromeos::BalloonViewImpl* GetBalloonViewOf(const Balloon* balloon) {
  return static_cast<chromeos::BalloonViewImpl*>(balloon->view());
}

// A Widget that covers entire ScrollView's viewport. Without this, all
// renderer's native gtk widgets are moved one by one via
// View::VisibleBoundsInRootChanged() notification, which makes scrolling not
// smooth.
class ViewportWidget : public views::Widget {
 public:
  explicit ViewportWidget(chromeos::NotificationPanel* panel)
      : panel_(panel),
        pointer_inside_panel_(false) {
  }

  void UpdateControl() {
    if (pointer_inside_panel_)
      panel_->OnMouseMotion(last_point_);
  }

  // views::Widget overrides:
  virtual bool OnMouseEvent(const views::MouseEvent& event) OVERRIDE {
    bool result = Widget::OnMouseEvent(event);
    switch (event.type()) {
      case ui::ET_MOUSE_MOVED:
        OnMouseMoved(event);
        break;
      case ui::ET_MOUSE_EXITED:
        OnMouseExited(event);
        break;
      default:
        break;
    }
    return result;
  }

 private:
  void OnMouseMoved(const views::MouseEvent& event) {
    // Convert the point to the panel's coordinate system.
    Widget* top_level_widget = GetTopLevelWidget();
    // TODO: remove cast when this extends Widget.
    DCHECK(top_level_widget != static_cast<Widget*>(this));
    gfx::Rect panel_rect(event.location(), gfx::Size());
    Widget::ConvertRect(this, top_level_widget, &panel_rect);
    last_point_ = panel_rect.origin();
    pointer_inside_panel_ = true;
    panel_->OnMouseMotion(last_point_);
  }

  void OnMouseExited(const views::MouseEvent& event) {
    // Filter gtk's fake mouse leave events that are issued when
    // window activation changes.
    if (event.location().x() == 0 && event.location().y() == 0)
      return;

    // Leave notify can happen if the mouse moves into the child gdk window.
    // Make sure the mouse is outside of the panel.
    gfx::Rect bounds = GetWindowScreenBounds();
    if (!bounds.Contains(event.location().x() + bounds.x(),
                         event.location().y() + bounds.y())) {
      panel_->OnMouseLeave();
      pointer_inside_panel_ = false;
    }
  }

 private:
  chromeos::NotificationPanel* panel_;
  gfx::Point last_point_;
  bool pointer_inside_panel_;

  DISALLOW_COPY_AND_ASSIGN(ViewportWidget);
};

class BalloonSubContainer : public views::View {
 public:
  explicit BalloonSubContainer(int margin)
      : margin_(margin) {
  }

  virtual ~BalloonSubContainer() {}

  // views::View overrides.
  virtual gfx::Size GetPreferredSize() {
    return preferred_size_;
  }

  virtual void Layout() {
    // Layout bottom up
    int height = 0;
    for (int i = child_count() - 1; i >= 0; --i) {
      views::View* child = child_at(i);
      child->SetBounds(0, height, child->width(), child->height());
      height += child->height() + margin_;
    }
    SchedulePaint();
  }

  // Updates the bound so that it can show all balloons.
  void UpdateBounds() {
    int height = 0;
    int max_width = 0;
    for (int i = child_count() - 1; i >= 0; --i) {
      views::View* child = child_at(i);
      height += child->height() + margin_;
      max_width = std::max(max_width, child->width());
    }
    if (height > 0)
      height -= margin_;
    preferred_size_.set_width(max_width);
    preferred_size_.set_height(height);
    SizeToPreferredSize();
  }

  // Returns the bounds that covers new notifications.
  gfx::Rect GetNewBounds() {
    gfx::Rect rect;
    for (int i = child_count() - 1; i >= 0; --i) {
      BalloonViewImpl* view = static_cast<BalloonViewImpl*>(child_at(i));
      if (!view->stale()) {
        if (rect.IsEmpty()) {
          rect = view->bounds();
        } else {
          rect = rect.Union(view->bounds());
        }
      }
    }
    return gfx::Rect(x(), y(), rect.width(), rect.height());
  }

  // Returns # of new notifications.
  int GetNewCount() {
    int count = 0;
    for (int i = child_count() - 1; i >= 0; --i) {
      BalloonViewImpl* view = static_cast<BalloonViewImpl*>(child_at(i));
      if (!view->stale())
        count++;
    }
    return count;
  }

  // Make all notifications stale.
  void MakeAllStale() {
    for (int i = child_count() - 1; i >= 0; --i) {
      BalloonViewImpl* view = static_cast<BalloonViewImpl*>(child_at(i));
      view->set_stale();
    }
  }

  void DismissAll() {
    for (int i = child_count() - 1; i >= 0; --i) {
      BalloonViewImpl* view = static_cast<BalloonViewImpl*>(child_at(i));
      view->Close(true);
    }
  }

  BalloonViewImpl* FindBalloonView(const Notification& notification) {
    for (int i = child_count() - 1; i >= 0; --i) {
      BalloonViewImpl* view = static_cast<BalloonViewImpl*>(child_at(i));
      if (view->IsFor(notification)) {
        return view;
      }
    }
    return NULL;
  }

  BalloonViewImpl* FindBalloonView(const gfx::Point point) {
    gfx::Point copy(point);
    ConvertPointFromWidget(this, &copy);
    for (int i = child_count() - 1; i >= 0; --i) {
      views::View* view = child_at(i);
      if (view->bounds().Contains(copy))
        return static_cast<BalloonViewImpl*>(view);
    }
    return NULL;
  }

  // Returns true if the |view| is in the container.
  // |view| can be a deleted pointer - we do not dereference it.
  bool HasChildView(View* view) const {
    for (int i = 0; i < child_count(); ++i) {
      if (child_at(i) == view)
        return true;
    }
    return false;
  }

 private:
  gfx::Size preferred_size_;
  int margin_;

  DISALLOW_COPY_AND_ASSIGN(BalloonSubContainer);
};

}  // namespace

namespace chromeos {

class BalloonContainer : public views::View {
 public:
  explicit BalloonContainer(int margin)
      : margin_(margin),
        sticky_container_(new BalloonSubContainer(margin)),
        non_sticky_container_(new BalloonSubContainer(margin)) {
    AddChildView(sticky_container_);
    AddChildView(non_sticky_container_);
  }
  virtual ~BalloonContainer() {}

  // views::View overrides.
  virtual void Layout() {
    int margin =
        (sticky_container_->child_count() != 0 &&
         non_sticky_container_->child_count() != 0) ?
        margin_ : 0;
    sticky_container_->SetBounds(
        0, 0, width(), sticky_container_->height());
    non_sticky_container_->SetBounds(
        0, sticky_container_->bounds().bottom() + margin,
        width(), non_sticky_container_->height());
  }

  virtual gfx::Size GetPreferredSize() {
    return preferred_size_;
  }

  // Returns the size that covers sticky and new notifications.
  gfx::Size GetStickyNewSize() {
    gfx::Rect sticky = sticky_container_->bounds();
    gfx::Rect new_non_sticky = non_sticky_container_->GetNewBounds();
    if (sticky.IsEmpty())
      return new_non_sticky.size();
    if (new_non_sticky.IsEmpty())
      return sticky.size();
    return sticky.Union(new_non_sticky).size();
  }

  // Adds a ballon to the panel.
  void Add(Balloon* balloon) {
    BalloonViewImpl* view = GetBalloonViewOf(balloon);
    GetContainerFor(balloon)->AddChildView(view);
  }

  // Updates the position of the |balloon|.
  bool Update(Balloon* balloon) {
    BalloonViewImpl* view = GetBalloonViewOf(balloon);
    View* container = NULL;
    if (view->parent() == sticky_container_) {
      container = sticky_container_;
    } else if (view->parent() == non_sticky_container_) {
      container = non_sticky_container_;
    }
    if (container) {
      container->RemoveChildView(view);
      container->AddChildView(view);
      return true;
    } else {
      return false;
    }
  }

  // Removes a ballon from the panel.
  BalloonViewImpl* Remove(Balloon* balloon) {
    BalloonViewImpl* view = GetBalloonViewOf(balloon);
    GetContainerFor(balloon)->RemoveChildView(view);
    return view;
  }

  // Returns the number of notifications added to the panel.
  int GetNotificationCount() {
    return sticky_container_->child_count() +
        non_sticky_container_->child_count();
  }

  // Returns the # of new notifications.
  int GetNewNotificationCount() {
    return sticky_container_->GetNewCount() +
        non_sticky_container_->GetNewCount();
  }

  // Returns the # of sticky and new notifications.
  int GetStickyNewNotificationCount() {
    return sticky_container_->child_count() +
        non_sticky_container_->GetNewCount();
  }

  // Returns the # of sticky notifications.
  int GetStickyNotificationCount() {
    return sticky_container_->child_count();
  }

  // Returns true if the |view| is contained in the panel.
  // |view| can be a deleted pointer - we do not dereference it.
  bool HasBalloonView(View* view) {
    return sticky_container_->HasChildView(view) ||
        non_sticky_container_->HasChildView(view);
  }

  // Updates the bounds so that all notifications are visible.
  void UpdateBounds() {
    sticky_container_->UpdateBounds();
    non_sticky_container_->UpdateBounds();
    preferred_size_ = sticky_container_->GetPreferredSize();

    gfx::Size non_sticky_size = non_sticky_container_->GetPreferredSize();
    int margin =
        (!preferred_size_.IsEmpty() && !non_sticky_size.IsEmpty()) ?
        margin_ : 0;
    preferred_size_.Enlarge(0, non_sticky_size.height() + margin);
    preferred_size_.set_width(std::max(
        preferred_size_.width(), non_sticky_size.width()));
    SizeToPreferredSize();
  }

  void MakeAllStale() {
    sticky_container_->MakeAllStale();
    non_sticky_container_->MakeAllStale();
  }

  void DismissAllNonSticky() {
    non_sticky_container_->DismissAll();
  }

  BalloonViewImpl* FindBalloonView(const Notification& notification) {
    BalloonViewImpl* view = sticky_container_->FindBalloonView(notification);
    return view ? view : non_sticky_container_->FindBalloonView(notification);
  }

  BalloonViewImpl* FindBalloonView(const gfx::Point& point) {
    BalloonViewImpl* view = sticky_container_->FindBalloonView(point);
    return view ? view : non_sticky_container_->FindBalloonView(point);
  }

 private:
  BalloonSubContainer* GetContainerFor(Balloon* balloon) const {
    BalloonViewImpl* view = GetBalloonViewOf(balloon);
    return view->sticky() ?
        sticky_container_ : non_sticky_container_;
  }

  int margin_;
  // Sticky/non-sticky ballon containers. They're child views and
  // deleted when this container is deleted.
  BalloonSubContainer* sticky_container_;
  BalloonSubContainer* non_sticky_container_;
  gfx::Size preferred_size_;

  DISALLOW_COPY_AND_ASSIGN(BalloonContainer);
};

NotificationPanel::NotificationPanel()
    : balloon_container_(NULL),
      panel_widget_(NULL),
      container_host_(NULL),
      state_(CLOSED),
      min_bounds_(0, 0, kBalloonMinWidth, kBalloonMinHeight),
      stale_timeout_(1000 * kStaleTimeoutInSeconds),
      active_(NULL),
      scroll_to_(NULL) {
  Init();
}

NotificationPanel::~NotificationPanel() {
  Hide();
}

////////////////////////////////////////////////////////////////////////////////
// NotificationPanel public.

void NotificationPanel::Show() {
  if (!panel_widget_) {
    panel_widget_ = new views::Widget;
    // TODO(oshima): Using window because Popup widget behaves weird
    // when resizing. This needs to be investigated.
    Widget::InitParams params(Widget::InitParams::TYPE_WINDOW_FRAMELESS);
    // Enable double buffering because the panel has both pure views
    // control and native controls (scroll bar).
    params.double_buffer = true;

    gfx::Rect bounds = GetPreferredBounds();
    bounds = bounds.Union(min_bounds_);
    params.bounds = bounds;
    panel_widget_->Init(params);
    // Set minimum bounds so that it can grow freely.
    gtk_widget_set_size_request(GTK_WIDGET(panel_widget_->GetNativeView()),
                                min_bounds_.width(), min_bounds_.height());

    views::NativeViewHost* native = new views::NativeViewHost();
    scroll_view_->SetContents(native);

    panel_widget_->SetContentsView(scroll_view_.get());

#if defined(USE_AURA)
    native->AttachToView(balloon_container_.get());
#else
    // Add the view port after scroll_view is attached to the panel widget.
    ViewportWidget* viewport_widget = new ViewportWidget(this);
    container_host_ = viewport_widget;
    container_host_->Init(
        views::Widget::InitParams(views::Widget::InitParams::TYPE_CONTROL));
    container_host_->SetContentsView(balloon_container_.get());
    // The window_contents_ is onwed by the Widget. Increase ref count
    // so that window_contents does not get deleted when detached.
    g_object_ref(viewport_widget->GetNativeView());
    native->Attach(viewport_widget->GetNativeView());
#endif
    UnregisterNotification();
    panel_controller_.reset(
        new PanelController(this, GTK_WINDOW(panel_widget_->GetNativeView())));
    panel_controller_->Init(false /* don't focus when opened */,
                            gfx::Rect(0, 0, kBalloonMinWidth, 1), 0,
                            WM_IPC_PANEL_USER_RESIZE_VERTICALLY);
    registrar_.Add(this, chrome::NOTIFICATION_PANEL_STATE_CHANGED,
                   content::Source<PanelController>(panel_controller_.get()));
  }
  panel_widget_->Show();
}

void NotificationPanel::Hide() {
  balloon_container_->DismissAllNonSticky();
  if (panel_widget_) {
    if (container_host_)
      container_host_->GetRootView()->RemoveChildView(balloon_container_.get());

    views::NativeViewHost* native =
        static_cast<views::NativeViewHost*>(scroll_view_->GetContents());
    native->Detach();
    scroll_view_->SetContents(NULL);
    if (container_host_) {
      container_host_->Hide();
      container_host_->CloseNow();
      container_host_ = NULL;
    }

    UnregisterNotification();
    panel_controller_->Close();
    MessageLoop::current()->DeleteSoon(FROM_HERE, panel_controller_.release());
    // We need to remove & detach the scroll view from hierarchy to
    // avoid GTK deleting child.
    // TODO(oshima): handle this details in NativeWidgetGtk.
    panel_widget_->GetRootView()->RemoveChildView(scroll_view_.get());
    panel_widget_->Close();
    panel_widget_ = NULL;
  }
}

////////////////////////////////////////////////////////////////////////////////
// BalloonCollectionImpl::NotificationUI overrides.

void NotificationPanel::Add(Balloon* balloon) {
  balloon_container_->Add(balloon);
  if (state_ == CLOSED || state_ == MINIMIZED)
    SET_STATE(STICKY_AND_NEW);
  Show();
  // Don't resize the panel yet. The panel will be resized when WebKit tells
  // the size in ResizeNotification.
  UpdatePanel(false);
  UpdateControl();
  StartStaleTimer(balloon);
  scroll_to_ = balloon;
}

bool NotificationPanel::Update(Balloon* balloon) {
  return balloon_container_->Update(balloon);
}

void NotificationPanel::Remove(Balloon* balloon) {
  BalloonViewImpl* view = balloon_container_->Remove(balloon);
  if (view == active_)
    active_ = NULL;
  if (scroll_to_ == balloon)
    scroll_to_ = NULL;

  // TODO(oshima): May be we shouldn't close
  // if the mouse pointer is still on the panel.
  if (balloon_container_->GetNotificationCount() == 0)
    SET_STATE(CLOSED);
  // no change to the state
  if (state_ == KEEP_SIZE) {
    // Just update the content.
    UpdateContainerBounds();
  } else {
    if (state_ != CLOSED &&
        balloon_container_->GetStickyNewNotificationCount() == 0)
      SET_STATE(MINIMIZED);
    UpdatePanel(true);
  }
  UpdateControl();
}

void NotificationPanel::Show(Balloon* balloon) {
  if (state_ == CLOSED || state_ == MINIMIZED)
    SET_STATE(STICKY_AND_NEW);
  Show();
  UpdatePanel(true);
  StartStaleTimer(balloon);
  ScrollBalloonToVisible(balloon);
}

void NotificationPanel::ResizeNotification(
    Balloon* balloon, const gfx::Size& size) {
  // restrict to the min & max sizes
  gfx::Size real_size(
      std::max(kBalloonMinWidth,
               std::min(kBalloonMaxWidth, size.width())),
      std::max(kBalloonMinHeight,
               std::min(kBalloonMaxHeight, size.height())));

  // Don't allow balloons to shrink.  This avoids flickering
  // which sometimes rapidly reports alternating sizes.  Special
  // case for setting the minimum value.
  gfx::Size old_size = balloon->content_size();
  if (real_size.width() > old_size.width() ||
      real_size.height() > old_size.height() ||
      real_size == min_bounds_.size()) {
    balloon->set_content_size(real_size);
    GetBalloonViewOf(balloon)->Layout();
    UpdatePanel(true);
    if (scroll_to_ == balloon) {
      ScrollBalloonToVisible(scroll_to_);
      scroll_to_ = NULL;
    }
  }
}

void NotificationPanel::SetActiveView(BalloonViewImpl* view) {
  // Don't change the active view if it's same notification,
  // or the notification is being closed.
  if (active_ == view || (view && view->closed()))
    return;
  if (active_)
    active_->Deactivated();
  active_ = view;
  if (active_)
    active_->Activated();
}

////////////////////////////////////////////////////////////////////////////////
// PanelController overrides.

string16 NotificationPanel::GetPanelTitle() {
  return string16(l10n_util::GetStringUTF16(IDS_NOTIFICATION_PANEL_TITLE));
}

SkBitmap NotificationPanel::GetPanelIcon() {
  return SkBitmap();
}

bool NotificationPanel::CanClosePanel() {
  return true;
}

void NotificationPanel::ClosePanel() {
  SET_STATE(CLOSED);
  UpdatePanel(false);
}

void NotificationPanel::ActivatePanel() {
  if (active_)
    active_->Activated();
}

////////////////////////////////////////////////////////////////////////////////
// content::NotificationObserver overrides.

void NotificationPanel::Observe(int type,
                                const content::NotificationSource& source,
                                const content::NotificationDetails& details) {
  DCHECK(type == chrome::NOTIFICATION_PANEL_STATE_CHANGED);
  PanelController::State* state =
      reinterpret_cast<PanelController::State*>(details.map_key());
  switch (*state) {
    case PanelController::EXPANDED:
      // Geting expanded in STICKY_AND_NEW or in KEEP_SIZE state means
      // that a new notification is added, so just leave the
      // state. Otherwise, expand to full.
      if (state_ != STICKY_AND_NEW && state_ != KEEP_SIZE)
        SET_STATE(FULL);
      // When the panel is to be expanded, we either show all, or
      // show only sticky/new, depending on the state.
      UpdatePanel(false);
      break;
    case PanelController::MINIMIZED:
      SET_STATE(MINIMIZED);
      // Make all notifications stale when a user minimize the panel.
      balloon_container_->MakeAllStale();
      break;
    case PanelController::INITIAL:
      NOTREACHED() << "Transition to Initial state should not happen";
  }
}

////////////////////////////////////////////////////////////////////////////////
// PanelController public.

void NotificationPanel::OnMouseLeave() {
  SetActiveView(NULL);
  if (balloon_container_->GetNotificationCount() == 0)
    SET_STATE(CLOSED);
  UpdatePanel(true);
}

void NotificationPanel::OnMouseMotion(const gfx::Point& point) {
  SetActiveView(balloon_container_->FindBalloonView(point));
  // We need to set the focus to scroll view to get mouse wheel
  // working. Setting focus when mouse moves on the panel
  // because focus may be taken by other view.
  scroll_view_->RequestFocus();
  // This method used to set KEEP_SIZE state. However,
  // some html notifications may want to change their size,
  // and setting KEEP_SIZE caused them to behave differently
  // depending on whether user moved mouse over notification
  // or not.
}

NotificationPanelTester* NotificationPanel::GetTester() {
  if (!tester_.get())
    tester_.reset(new NotificationPanelTester(this));
  return tester_.get();
}

////////////////////////////////////////////////////////////////////////////////
// NotificationPanel private.

void NotificationPanel::Init() {
  DCHECK(!panel_widget_);
  balloon_container_.reset(new BalloonContainer(1));
  balloon_container_->set_parent_owned(false);
  balloon_container_->set_background(
      views::Background::CreateSolidBackground(kPanelBackground));

  scroll_view_.reset(new views::ScrollView());
  scroll_view_->set_focusable(true);
  scroll_view_->set_parent_owned(false);
  scroll_view_->set_background(
      views::Background::CreateSolidBackground(SK_ColorWHITE));
}

void NotificationPanel::UnregisterNotification() {
  if (panel_controller_.get()) {
    registrar_.Remove(
        this, chrome::NOTIFICATION_PANEL_STATE_CHANGED,
        content::Source<PanelController>(panel_controller_.get()));
  }
}

void NotificationPanel::ScrollBalloonToVisible(Balloon* balloon) {
  BalloonViewImpl* view = GetBalloonViewOf(balloon);
  if (!view->closed()) {
    // We can't use View::ScrollRectToVisible because the viewport is not
    // ancestor of the BalloonViewImpl.
    // Use Widget's coordinate which is same as viewport's coordinates.
    gfx::Point p(0, 0);
    views::View::ConvertPointToWidget(view, &p);
    gfx::Rect visible_rect(p.x(), p.y(), view->width(), view->height());
    scroll_view_->ScrollContentsRegionToBeVisible(visible_rect);
  }
}

void NotificationPanel::UpdatePanel(bool update_container_size) {
  if (update_container_size)
    UpdateContainerBounds();
  switch (state_) {
    case KEEP_SIZE: {
      gfx::Rect min_bounds = GetPreferredBounds();
      gfx::Rect panel_bounds = panel_widget_->GetWindowScreenBounds();
      if (min_bounds.height() < panel_bounds.height())
        panel_widget_->SetBounds(min_bounds);
      else if (min_bounds.height() > panel_bounds.height()) {
        // need scroll bar
        int width = balloon_container_->width() +
            scroll_view_->GetScrollBarWidth();
        panel_bounds.set_width(width);
        panel_widget_->SetBounds(panel_bounds);
      }

      // no change.
      break;
    }
    case CLOSED:
      Hide();
      break;
    case MINIMIZED:
      balloon_container_->MakeAllStale();
      if (panel_controller_.get())
        panel_controller_->SetState(PanelController::MINIMIZED);
      break;
    case FULL:
      if (panel_widget_) {
        panel_widget_->SetBounds(GetPreferredBounds());
        panel_controller_->SetState(PanelController::EXPANDED);
      }
      break;
    case STICKY_AND_NEW:
      if (panel_widget_) {
        panel_widget_->SetBounds(GetStickyNewBounds());
        panel_controller_->SetState(PanelController::EXPANDED);
      }
      break;
  }
}

void NotificationPanel::UpdateContainerBounds() {
  balloon_container_->UpdateBounds();
  views::NativeViewHost* native =
      static_cast<views::NativeViewHost*>(scroll_view_->GetContents());
  // Update from WebKit may arrive after the panel is closed/hidden
  // and viewport widget is detached.
  if (native) {
    native->SetBoundsRect(balloon_container_->bounds());
    scroll_view_->Layout();
  }
}

void NotificationPanel::UpdateControl() {
  if (container_host_)
    static_cast<ViewportWidget*>(container_host_)->UpdateControl();
}

gfx::Rect NotificationPanel::GetPreferredBounds() {
  gfx::Size pref_size = balloon_container_->GetPreferredSize();
  int new_height = std::min(pref_size.height(), kMaxPanelHeight);
  int new_width = pref_size.width();
  // Adjust the width to avoid showing a horizontal scroll bar.
  if (new_height != pref_size.height()) {
    new_width += scroll_view_->GetScrollBarWidth();
  }
  return gfx::Rect(0, 0, new_width, new_height).Union(min_bounds_);
}

gfx::Rect NotificationPanel::GetStickyNewBounds() {
  gfx::Size pref_size = balloon_container_->GetPreferredSize();
  gfx::Size sticky_size = balloon_container_->GetStickyNewSize();
  int new_height = std::min(sticky_size.height(), kMaxPanelHeight);
  int new_width = pref_size.width();
  // Adjust the width to avoid showing a horizontal scroll bar.
  if (new_height != pref_size.height())
    new_width += scroll_view_->GetScrollBarWidth();
  return gfx::Rect(0, 0, new_width, new_height).Union(min_bounds_);
}

void NotificationPanel::StartStaleTimer(Balloon* balloon) {
  BalloonViewImpl* view = GetBalloonViewOf(balloon);
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      base::Bind(&NotificationPanel::OnStale, AsWeakPtr(), view),
      stale_timeout_);
}

void NotificationPanel::OnStale(BalloonViewImpl* view) {
  // Note: |view| may point to deleted memory.
  if (balloon_container_->HasBalloonView(view) && !view->stale()) {
    view->set_stale();
    // don't update panel on stale
    if (state_ == KEEP_SIZE)
      return;
    if (balloon_container_->GetStickyNewNotificationCount() > 0) {
      SET_STATE(STICKY_AND_NEW);
    } else {
      SET_STATE(MINIMIZED);
    }
    UpdatePanel(false);
  }
}

void NotificationPanel::SetState(State new_state, const char* name) {
#if !defined(NDEBUG)
  DVLOG(1) << "state transition " << ToStr(state_) << " >> " << ToStr(new_state)
           << " in " << name;
#endif
  state_ = new_state;
}

void NotificationPanel::MarkStale(const Notification& notification) {
  BalloonViewImpl* view = balloon_container_->FindBalloonView(notification);
  DCHECK(view);
  OnStale(view);
}

////////////////////////////////////////////////////////////////////////////////
// NotificationPanelTester public.

int NotificationPanelTester::GetNotificationCount() const {
  return panel_->balloon_container_->GetNotificationCount();
}

int NotificationPanelTester::GetStickyNotificationCount() const {
  return panel_->balloon_container_->GetStickyNotificationCount();
}

int NotificationPanelTester::GetNewNotificationCount() const {
  return panel_->balloon_container_->GetNewNotificationCount();
}

void NotificationPanelTester::SetStaleTimeout(int timeout) {
  panel_->stale_timeout_ = timeout;
}

void NotificationPanelTester::MarkStale(const Notification& notification) {
  panel_->MarkStale(notification);
}

PanelController* NotificationPanelTester::GetPanelController() const {
  return panel_->panel_controller_.get();
}

BalloonViewImpl* NotificationPanelTester::GetBalloonView(
    BalloonCollectionImpl* collection,
    const Notification& notification) {
  Balloon* balloon = collection->FindBalloon(notification);
  DCHECK(balloon);
  return GetBalloonViewOf(balloon);
}

bool NotificationPanelTester::IsVisible(const BalloonViewImpl* view) const {
  gfx::Rect rect = panel_->scroll_view_->GetVisibleRect();
  gfx::Point origin(0, 0);
  views::View::ConvertPointToView(view, panel_->balloon_container_.get(),
                                  &origin);
  return rect.Contains(gfx::Rect(origin, view->size()));
}


bool NotificationPanelTester::IsActive(const BalloonViewImpl* view) const {
  return panel_->active_ == view;
}

}  // namespace chromeos
