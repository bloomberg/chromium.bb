// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Draws the view for the balloons.

#include "chrome/browser/chromeos/notifications/notification_panel.h"

#include "app/gfx/canvas.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "chrome/browser/chromeos/notifications/balloon_view.h"
#include "grit/generated_resources.h"
#include "views/background.h"
#include "views/controls/scroll_view.h"
#include "views/widget/root_view.h"
#include "views/widget/widget_gtk.h"

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

using chromeos::BalloonViewImpl;

class PanelWidget : public views::WidgetGtk {
 public:
  explicit PanelWidget(chromeos::NotificationPanel* panel)
      : WidgetGtk(views::WidgetGtk::TYPE_WINDOW),
        panel_(panel) {
  }

  // views::WidgetGtk overrides.
  virtual gboolean OnMotionNotify(GtkWidget* widget, GdkEventMotion* event) {
    gboolean result = WidgetGtk::OnMotionNotify(widget, event);
    panel_->DontUpdatePanelOnStale();
    return result;
  }

  virtual gboolean OnLeaveNotify(GtkWidget* widget, GdkEventCrossing* event) {
    gboolean result = views::WidgetGtk::OnLeaveNotify(widget, event);
    // Leave notify can happen if the mouse moves into the child gdk window.
    // Make sure the mouse is outside of the panel.
    gfx::Point p(event->x_root, event->y_root);
    gfx::Rect bounds;
    GetBounds(&bounds, true);
    if (!bounds.Contains(p)) {
      panel_->OnMouseLeave();
    }
    return result;
  }

 private:
  chromeos::NotificationPanel* panel_;
  DISALLOW_COPY_AND_ASSIGN(PanelWidget);
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
    int count = GetChildViewCount();
    int height = 0;
    for (int i = count - 1; i >= 0; --i) {
      views::View* child = GetChildViewAt(i);
      child->SetBounds(0, height, child->width(), child->height());
      height += child->height() + margin_;
    }
    SchedulePaint();
  }

  // Updates the bound so that it can show all balloons.
  void UpdateBounds() {
    int height = 0;
    int max_width = 0;
    for (int i = GetChildViewCount() - 1; i >= 0; --i) {
      views::View* c = GetChildViewAt(i);
      height += c->height() + margin_;
      max_width = std::max(max_width, c->width());
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
    for (int i = GetChildViewCount() - 1; i >= 0; --i) {
      BalloonViewImpl* view =
          static_cast<BalloonViewImpl*>(GetChildViewAt(i));
      if (!view->stale()) {
        if (rect.IsEmpty()) {
          rect = view->bounds();
        } else {
          rect = rect.Union(bounds());
        }
      }
    }
    return gfx::Rect(x(), y(), rect.width(), rect.height());
  }

  // Returns # of new notifications.
  int GetNewCount() {
    int count = 0;
    for (int i = GetChildViewCount() - 1; i >= 0; --i) {
      BalloonViewImpl* view =
          static_cast<BalloonViewImpl*>(GetChildViewAt(i));
      if (!view->stale())
        count++;
    }
    return count;
  }

  // Make all notifications stale.
  void MakeAllStale() {
    for (int i = GetChildViewCount() - 1; i >= 0; --i) {
      BalloonViewImpl* view =
          static_cast<BalloonViewImpl*>(GetChildViewAt(i));
      view->set_stale();
    }
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
  BalloonContainer(int margin)
      : margin_(margin),
        sticky_container_(new BalloonSubContainer(margin)),
        non_sticky_container_(new BalloonSubContainer(margin)) {
    AddChildView(sticky_container_);
    AddChildView(non_sticky_container_);
  }

  // views::View overrides.
  virtual void Layout() {
    int margin =
        (sticky_container_->GetChildViewCount() != 0 &&
         non_sticky_container_->GetChildViewCount() != 0) ?
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
    gfx::Rect new_sticky = sticky_container_->bounds();
    gfx::Rect new_non_sticky = non_sticky_container_->GetNewBounds();
    if (new_sticky.IsEmpty())
      return new_non_sticky.size();
    if (new_non_sticky.IsEmpty())
      return new_sticky.size();
    return new_sticky.Union(new_non_sticky).size();
  }

  // Adds a ballon to the panel.
  void Add(Balloon* balloon) {
    BalloonViewImpl* view =
        static_cast<BalloonViewImpl*>(balloon->view());
    GetContainerFor(balloon)->AddChildView(view);
  }

  // Updates the position of the |balloon|.
  bool Update(Balloon* balloon) {
    BalloonViewImpl* view =
        static_cast<BalloonViewImpl*>(balloon->view());
    View* container = NULL;
    if (sticky_container_->HasChildView(view)) {
      container = sticky_container_;
    } else if (non_sticky_container_->HasChildView(view)) {
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
  void Remove(Balloon* balloon) {
    BalloonViewImpl* view =
        static_cast<BalloonViewImpl*>(balloon->view());
    GetContainerFor(balloon)->RemoveChildView(view);
  }

  // Returns the number of notifications added to the panel.
  int GetNotificationCount() {
    return sticky_container_->GetChildViewCount() +
        non_sticky_container_->GetChildViewCount();
  }

  // Returns the # of new notifications.
  bool GetNewNotificationCount() {
    return sticky_container_->GetNewCount() +
        non_sticky_container_->GetNewCount();
  }

  // Returns the # of sticky notifications.
  bool GetStickyNotificationCount() {
    return sticky_container_->GetChildViewCount();
  }

  // Returns true if the |view| is contained in the panel.
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

 private:
  BalloonSubContainer* GetContainerFor(Balloon* balloon) const {
    BalloonViewImpl* view =
        static_cast<BalloonViewImpl*>(balloon->view());
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
      state_(CLOSED),
      task_factory_(this),
      update_panel_on_mouse_leave_(false),
      latest_token_(0),
      stale_token_(0) {
  Init();
}

NotificationPanel::~NotificationPanel() {
  Hide();
}

////////////////////////////////////////////////////////////////////////////////
// NottificationPanel public.

void NotificationPanel::Show() {
  if (!panel_widget_.get()) {
    // TODO(oshima): Using window because Popup widget behaves weird
    // when resizing. This needs to be investigated.
    panel_widget_.reset(new PanelWidget(this));
    gfx::Rect bounds = GetPreferredBounds();
    if (bounds.width() < kBalloonMinWidth ||
        bounds.height() < kBalloonMinHeight) {
      // Gtk uses its own default size when the size is empty.
      // Use the minimum size as a default.
      bounds.SetRect(0, 0, kBalloonMinWidth, kBalloonMinHeight);
    }
    panel_widget_->Init(NULL, bounds);
    // TODO(oshima): I needed the following code in order to get sizing
    // reliably. Investigate and fix it in WidgetGtk.
    gtk_widget_set_size_request(GTK_WIDGET(panel_widget_->GetNativeView()),
                                bounds.width(), bounds.height());
    panel_widget_->SetContentsView(scroll_view_.get());
    panel_controller_.reset(
        new PanelController(this,
                            GTK_WINDOW(panel_widget_->GetNativeView()),
                            gfx::Rect(0, 0, kBalloonMinWidth, 1)));
  }
  panel_widget_->Show();
}

void NotificationPanel::Hide() {
  if (panel_widget_.get()) {
    // We need to remove & detach the scroll view from hierarchy to
    // avoid GTK deleting child.
    // TODO(oshima): handle this details in WidgetGtk.
    panel_widget_->GetRootView()->RemoveChildView(scroll_view_.get());
    panel_widget_.release()->Close();
    panel_controller_.release()->Close();
  }
}

int NotificationPanel::GetStickyNotificationCount() const {
  return balloon_container_->GetStickyNotificationCount();
}

int NotificationPanel::GetNewNotificationCount() const {
  return balloon_container_->GetNewNotificationCount();
}

////////////////////////////////////////////////////////////////////////////////
// BalloonCollectionImpl::NotificationUI overrides.

void NotificationPanel::Add(Balloon* balloon) {
  balloon_container_->Add(balloon);
  if (state_ == CLOSED || state_ == MINIMIZED)
    state_ = STICKY_AND_NEW;
  Show();
  UpdatePanel(true);
  StartStaleTimer(balloon);
}

bool NotificationPanel::Update(Balloon* balloon) {
  if (balloon_container_->Update(balloon)) {
    if (state_ == CLOSED || state_ == MINIMIZED)
      state_ = STICKY_AND_NEW;
    Show();
    UpdatePanel(true);
    StartStaleTimer(balloon);
    return true;
  } else {
    return false;
  }
}

void NotificationPanel::Remove(Balloon* balloon) {
  balloon_container_->Remove(balloon);
  // no change to the state
  if (balloon_container_->GetNotificationCount() == 0)
    state_ = CLOSED;
  if (static_cast<BalloonViewImpl*>(balloon->view())->closed_by_user()) {
    DontUpdatePanelOnStale();
    balloon_container_->UpdateBounds();
    scroll_view_->Layout();
  } else {
    UpdatePanel(true);
  }
}

void NotificationPanel::ResizeNotification(
    Balloon* balloon, const gfx::Size& size) {
  // restrict to the min & max sizes
  gfx::Size real_size(
      std::max(kBalloonMinWidth,
               std::min(kBalloonMaxWidth, size.width())),
      std::max(kBalloonMinHeight,
               std::min(kBalloonMaxHeight, size.height())));
  balloon->set_content_size(real_size);
  static_cast<BalloonViewImpl*>(balloon->view())->Layout();
  UpdatePanel(true);
}

////////////////////////////////////////////////////////////////////////////////
// PanelController overrides.

string16 NotificationPanel::GetPanelTitle() {
  return string16(l10n_util::GetStringUTF16(IDS_NOTIFICATION_PANEL_TITLE));
}

SkBitmap NotificationPanel::GetPanelIcon() {
  return SkBitmap();
}

void NotificationPanel::ClosePanel() {
  state_ = CLOSED;
  UpdatePanel(false);
}

void NotificationPanel::OnPanelStateChanged(PanelController::State state) {
  switch (state) {
    case PanelController::EXPANDED:
      // Geting expanded in STICKY_AND_NEW state means that a new
      // notification is added, so just leave the state. Otherwise,
      // expand to full.
      if (state_ != STICKY_AND_NEW)
        state_ = FULL;
      // When the panel is to be expanded, we either show all, or
      // show only sticky/new, depending on the state.
      UpdatePanel(false);
      break;
    case PanelController::MINIMIZED:
      state_ = MINIMIZED;
      // Make all notifications stale when a user minimize the panel.
      balloon_container_->MakeAllStale();
      break;
  }
}

void NotificationPanel::OnMouseLeave() {
  if (update_panel_on_mouse_leave_) {
    // TODO(oshima): We need "AS_IS" state, which simply
    // keeps the current panel size unless it has less notifications
    // than it can show.
    if (balloon_container_->GetStickyNotificationCount() > 0 ||
        balloon_container_->GetNewNotificationCount() > 0) {
      state_ = STICKY_AND_NEW;
    } else {
      state_ = MINIMIZED;
    }
    UpdatePanel(true);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NotificationPanel private.

void NotificationPanel::Init() {
  DCHECK(!panel_widget_.get());
  balloon_container_ = new BalloonContainer(1);
  balloon_container_->set_background(
      views::Background::CreateSolidBackground(ResourceBundle::frame_color));

  scroll_view_.reset(new views::ScrollView());
  scroll_view_->set_parent_owned(false);
  scroll_view_->SetContents(balloon_container_);
  scroll_view_->set_background(
      views::Background::CreateSolidBackground(SK_ColorWHITE));
}

void NotificationPanel::UpdatePanel(bool contents_changed) {
  update_panel_on_mouse_leave_ = false;
  if (contents_changed) {
    balloon_container_->UpdateBounds();
    scroll_view_->Layout();
  }
  switch(state_) {
    case CLOSED:
      balloon_container_->MakeAllStale();
      Hide();
      break;
    case MINIMIZED:
      balloon_container_->MakeAllStale();
      if (panel_controller_.get())
        panel_controller_->SetState(PanelController::MINIMIZED);
      break;
    case FULL:
      if (panel_widget_.get()) {
        panel_widget_->SetBounds(GetPreferredBounds());
        panel_controller_->SetState(PanelController::EXPANDED);
      }
      break;
    case STICKY_AND_NEW:
      if (panel_widget_.get()) {
        panel_widget_->SetBounds(GetStickyNewBounds());
        panel_controller_->SetState(PanelController::EXPANDED);
      }
      break;
  }
}

gfx::Rect NotificationPanel::GetPreferredBounds() {
  gfx::Size pref_size = balloon_container_->GetPreferredSize();
  int new_height = std::min(pref_size.height(), kMaxPanelHeight);
  int new_width = pref_size.width();
  // Adjust the width to avoid showing a horizontal scroll bar.
  if (new_height != pref_size.height())
    new_width += scroll_view_->GetScrollBarWidth();
  return gfx::Rect(0, 0, new_width, new_height);
}

gfx::Rect NotificationPanel::GetStickyNewBounds() {
  gfx::Size pref_size = balloon_container_->GetPreferredSize();
  gfx::Size sticky_size = balloon_container_->GetStickyNewSize();
  int new_height = std::min(sticky_size.height(), kMaxPanelHeight);
  int new_width = pref_size.width();
  // Adjust the width to avoid showing a horizontal scroll bar.
  if (new_height != pref_size.height())
    new_width += scroll_view_->GetScrollBarWidth();
  return gfx::Rect(0, 0, new_width, new_height);
}

void NotificationPanel::DontUpdatePanelOnStale() {
  if (stale_token_ != latest_token_) {
    stale_token_ = latest_token_;
  }
  update_panel_on_mouse_leave_ = true;
}

void NotificationPanel::StartStaleTimer(Balloon* balloon) {
  BalloonViewImpl* view = static_cast<BalloonViewImpl*>(balloon->view());
  MessageLoop::current()->PostDelayedTask(
      FROM_HERE,
      task_factory_.NewRunnableMethod(
          &NotificationPanel::OnStale, view, latest_token_++),
      1000 * kStaleTimeoutInSeconds);
}

void NotificationPanel::OnStale(BalloonViewImpl* view, int token) {
  if (balloon_container_->HasBalloonView(view) && !view->stale()) {
    view->set_stale();
    // don't update panel on stale
    if (token < stale_token_) {
      return;
    }
    if (balloon_container_->GetStickyNotificationCount() > 0) {
      state_ = STICKY_AND_NEW;
    } else {
      state_ = MINIMIZED;
    }
    UpdatePanel(false);
  }
}

}  // namespace chromeos
