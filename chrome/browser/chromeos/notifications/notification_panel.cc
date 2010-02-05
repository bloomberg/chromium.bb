// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Draws the view for the balloons.

#include "chrome/browser/chromeos/notifications/notification_panel.h"

#include "app/gfx/canvas.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/singleton.h"
#include "chrome/browser/chromeos/notifications/balloon_view.h"
#include "grit/generated_resources.h"
#include "views/background.h"
#include "views/widget/widget_gtk.h"

namespace chromeos {

class BalloonContainer : public views::View {
 public:
  explicit BalloonContainer(int margin)
      : View(),
        margin_(margin) {
  }

  virtual ~BalloonContainer() {}

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
    if (height > 0) height -= margin_;
    preferred_size_.set_width(max_width);
    preferred_size_.set_height(height);
    PreferredSizeChanged();
    SizeToPreferredSize();
  }

 private:
  gfx::Size preferred_size_;
  int margin_;
};

// static
NotificationPanel* NotificationPanel::Get() {
  return Singleton<NotificationPanel>::get();
}

NotificationPanel::NotificationPanel()
    : panel_widget_(NULL) {
  Init();
}

NotificationPanel::~NotificationPanel() {
}

gfx::Rect NotificationPanel::GetPanelBounds() {
  gfx::Size pref_size = balloon_container_->GetPreferredSize();
  int new_height = pref_size.height();
  return gfx::Rect(0, 0, pref_size.width(), new_height);
}

void NotificationPanel::Init() {
  DCHECK(!panel_widget_.get());
  balloon_container_ = new BalloonContainer(1);
  balloon_container_->set_parent_owned(false);
  balloon_container_->set_background(
      views::Background::CreateSolidBackground(ResourceBundle::frame_color));
}

void NotificationPanel::Add(BalloonViewImpl* view) {
  balloon_container_->AddChildView(view);
  balloon_container_->UpdateBounds();
  balloon_container_->Layout();
  if (panel_widget_.get()) {
    panel_widget_->SetBounds(GetPanelBounds());
  }
}

void NotificationPanel::Remove(BalloonViewImpl* view) {
  balloon_container_->RemoveChildView(view);
  balloon_container_->UpdateBounds();
  balloon_container_->Layout();
  if (panel_widget_.get()) {
    if (balloon_container_->GetChildViewCount() == 0) {
      Hide();
    } else {
      panel_widget_->SetBounds(GetPanelBounds());
    }
  }
}

void NotificationPanel::Show() {
  if (!panel_widget_.get()) {
    // TODO(oshima): Using window because Popup widget behaves weird
    // when resizing. This needs to be investigated.
    panel_widget_.reset(new views::WidgetGtk(views::WidgetGtk::TYPE_WINDOW));
    panel_widget_->Init(NULL, GetPanelBounds());
    panel_widget_->SetContentsView(balloon_container_);
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

string16 NotificationPanel::GetPanelTitle() {
  return string16(l10n_util::GetStringUTF16(IDS_NOTIFICATION_PANEL_TITLE));
}

SkBitmap NotificationPanel::GetPanelIcon() {
  return SkBitmap();
}

void NotificationPanel::ClosePanel() {
  panel_widget_.release()->Close();
}

}  // namespace chromeos
