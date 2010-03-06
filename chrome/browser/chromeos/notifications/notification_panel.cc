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

  // Add a ballon to the panel.
  void Add(Balloon* balloon) {
    BalloonViewImpl* view =
        static_cast<BalloonViewImpl*>(balloon->view());
    GetContainerFor(balloon)->AddChildView(view);
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

  // Update the bounds so that all notifications are visible.
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

 private:
  BalloonSubContainer* GetContainerFor(Balloon* balloon) const {
    return balloon->notification().sticky() ?
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
    : balloon_container_(NULL) {
  Init();
}

NotificationPanel::~NotificationPanel() {
}

////////////////////////////////////////////////////////////////////////////////
// NottificationPanel public.

void NotificationPanel::Show() {
  if (!panel_widget_.get()) {
    // TODO(oshima): Using window because Popup widget behaves weird
    // when resizing. This needs to be investigated.
    panel_widget_.reset(new views::WidgetGtk(views::WidgetGtk::TYPE_WINDOW));
    panel_widget_->Init(NULL, GetPreferredBounds());
    panel_widget_->SetContentsView(scroll_view_.get());
    panel_controller_.reset(
        new PanelController(this,
                            GTK_WINDOW(panel_widget_->GetNativeView()),
                            gfx::Rect(0, 0, 1000, 1000)));
  }
  panel_widget_->Show();
}

void NotificationPanel::Hide() {
  if (panel_widget_.get()) {
    panel_widget_.release()->Close();
    panel_controller_.release()->Close();
  }
}

////////////////////////////////////////////////////////////////////////////////
// BalloonCollectionImpl::NotificationUI overrides.

void NotificationPanel::Add(Balloon* balloon) {
  balloon_container_->Add(balloon);
  UpdateSize();
  Show();
}

void NotificationPanel::Remove(Balloon* balloon) {
  balloon_container_->Remove(balloon);
  UpdateSize();
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
  UpdateSize();
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
  panel_widget_.release()->Close();
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
}

void NotificationPanel::UpdateSize() {
  balloon_container_->UpdateBounds();
  scroll_view_->Layout();
  if (panel_widget_.get()) {
    if (balloon_container_->GetNotificationCount() == 0)
      Hide();
    else
      panel_widget_->SetBounds(GetPreferredBounds());
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

}  // namespace chromeos
