// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/tray/system_tray.h"

#include "ash/shell.h"
#include "ash/shell/panel_window.h"
#include "ash/shell_window_ids.h"
#include "ash/system/tray/tray_constants.h"
#include "ash/system/tray/system_tray_delegate.h"
#include "ash/system/tray/system_tray_item.h"
#include "ash/system/user/login_status.h"
#include "ash/wm/shadow_types.h"
#include "ash/wm/window_animations.h"
#include "base/logging.h"
#include "base/timer.h"
#include "base/utf_string_conversions.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColor.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/core/SkPath.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/skia_util.h"
#include "ui/views/border.h"
#include "ui/views/bubble/bubble_delegate.h"
#include "ui/views/controls/label.h"
#include "ui/views/layout/fill_layout.h"
#include "ui/views/layout/box_layout.h"
#include "ui/views/view.h"

namespace ash {

namespace {

const int kPaddingFromRightEdgeOfScreen = 10;
const int kPaddingFromBottomOfScreen = 10;

const int kAnimationDurationForPopupMS = 200;

const int kArrowHeight = 10;
const int kArrowWidth = 20;
const int kArrowPaddingFromRight = 20;

const int kShadowOffset = 3;
const int kShadowHeight = 3;

const int kLeftPadding = 4;
const int kBottomLineHeight = 1;

const SkColor kDarkColor = SkColorSetRGB(120, 120, 120);
const SkColor kLightColor = SkColorSetRGB(240, 240, 240);
const SkColor kShadowColor = SkColorSetARGB(25, 0, 0, 0);

const SkColor kTrayBackgroundColor = SkColorSetARGB(100, 0, 0, 0);
const SkColor kTrayBackgroundHover = SkColorSetARGB(150, 0, 0, 0);

// A view with some special behaviour for tray items:
// - changes background color on hover.
// - TODO: accessibility
class TrayItemContainer : public views::View {
 public:
  explicit TrayItemContainer(views::View* view) : hover_(false) {
    set_notify_enter_exit_on_child(true);
    set_border(view->border() ? views::Border::CreateEmptyBorder(0, 0, 0, 0) :
                                NULL);
    SetLayoutManager(new views::FillLayout);
    AddChildView(view);
  }

  virtual ~TrayItemContainer() {}

 private:
  // Overridden from views::View.
  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE {
    hover_ = true;
    SchedulePaint();
  }

  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE {
    hover_ = false;
    SchedulePaint();
  }

  virtual void OnPaintBackground(gfx::Canvas* canvas) OVERRIDE {
    views::View* view = child_at(0);
    if (!view->background()) {
      canvas->FillRect(gfx::Rect(size()),
          hover_ ? kHoverBackgroundColor : kBackgroundColor);
    } else {
      canvas->FillRect(gfx::Rect(view->x() + kShadowOffset, view->y(),
                                 view->width() - kShadowOffset, kShadowHeight),
                       kShadowColor);
    }
  }

  bool hover_;

  DISALLOW_COPY_AND_ASSIGN(TrayItemContainer);
};

class SystemTrayBubbleBackground : public views::Background {
 public:
  explicit SystemTrayBubbleBackground(views::View* owner)
      : owner_(owner) {
  }

  virtual ~SystemTrayBubbleBackground() {}

 private:
  // Overridden from views::Background.
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    views::View* last_view = NULL;
    for (int i = 0; i < owner_->child_count(); i++) {
      views::View* v = owner_->child_at(i);

      if (!v->border()) {
        canvas->DrawLine(gfx::Point(v->x() - 1, v->y() - 1),
            gfx::Point(v->x() + v->width() + 1, v->y() - 1),
            !last_view || last_view->border() ? kDarkColor : kLightColor);
        canvas->DrawLine(gfx::Point(v->x() - 1, v->y() - 1),
            gfx::Point(v->x() - 1, v->y() + v->height() + 1),
            kDarkColor);
        canvas->DrawLine(gfx::Point(v->x() + v->width(), v->y() - 1),
            gfx::Point(v->x() + v->width(), v->y() + v->height() + 1),
            kDarkColor);
      } else if (last_view && !last_view->border()) {
        canvas->DrawLine(gfx::Point(v->x() - 1, v->y() - 1),
            gfx::Point(v->x() + v->width() + 1, v->y() - 1),
            kDarkColor);
      }

      last_view = v;
    }
  }

  views::View* owner_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayBubbleBackground);
};

class SystemTrayBubbleBorder : public views::Border {
 public:
  explicit SystemTrayBubbleBorder(views::View* owner)
      : owner_(owner) {
  }

  virtual ~SystemTrayBubbleBorder() {}

 private:
  // Overridden from views::Border.
  virtual void Paint(const views::View& view,
                     gfx::Canvas* canvas) const OVERRIDE {
    // Draw a line first.
    int y = owner_->height() + 1;
    canvas->FillRect(gfx::Rect(kLeftPadding, y, owner_->width(),
                               kBottomLineHeight), kDarkColor);

    // Now, draw a shadow.
    canvas->FillRect(gfx::Rect(kLeftPadding + kShadowOffset, y,
                               owner_->width() - kShadowOffset, kShadowHeight),
                     kShadowColor);

    // Draw the arrow.
    int left_base_x = owner_->width() - kArrowPaddingFromRight - kArrowWidth;
    int left_base_y = y;
    int tip_x = left_base_x + kArrowWidth / 2;
    int tip_y = left_base_y + kArrowHeight;
    SkPath path;
    path.incReserve(4);
    path.moveTo(SkIntToScalar(left_base_x), SkIntToScalar(left_base_y));
    path.lineTo(SkIntToScalar(tip_x), SkIntToScalar(tip_y));
    path.lineTo(SkIntToScalar(left_base_x + kArrowWidth),
                SkIntToScalar(left_base_y));

    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(kBackgroundColor);
    canvas->sk_canvas()->drawPath(path, paint);

    // Now the draw the outline.
    paint.setStyle(SkPaint::kStroke_Style);
    paint.setColor(kDarkColor);
    canvas->sk_canvas()->drawPath(path, paint);
  }

  virtual void GetInsets(gfx::Insets* insets) const OVERRIDE {
    insets->Set(0, 0, kArrowHeight, 0);
  }

  views::View* owner_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayBubbleBorder);
};

class SystemTrayBackground : public views::Background {
 public:
  SystemTrayBackground() : hovering_(false) {}
  virtual ~SystemTrayBackground() {}

  void set_hovering(bool hover) { hovering_ = hover; }

 private:
  // Overridden from views::Background.
  virtual void Paint(gfx::Canvas* canvas, views::View* view) const OVERRIDE {
    SkPaint paint;
    paint.setAntiAlias(true);
    paint.setStyle(SkPaint::kFill_Style);
    paint.setColor(hovering_ ? kTrayBackgroundHover : kTrayBackgroundColor);
    SkPath path;
    gfx::Rect bounds(view->GetContentsBounds());
    SkScalar radius = SkIntToScalar(4);
    path.addRoundRect(gfx::RectToSkRect(bounds), radius, radius);
    canvas->sk_canvas()->drawPath(path, paint);
  }

  bool hovering_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayBackground);
};

}  // namespace

namespace internal {

class SystemTrayBubble : public views::BubbleDelegateView {
 public:
  SystemTrayBubble(ash::SystemTray* tray,
                   views::View* anchor,
                   std::vector<ash::SystemTrayItem*>& items,
                   bool detailed)
      : views::BubbleDelegateView(anchor, views::BubbleBorder::BOTTOM_RIGHT),
        tray_(tray),
        items_(items),
        detailed_(detailed),
        can_activate_(true),
        autoclose_delay_(0) {
    set_margin(0);
    set_parent_window(ash::Shell::GetInstance()->GetContainer(
        ash::internal::kShellWindowId_SettingBubbleContainer));
    set_notify_enter_exit_on_child(true);
  }

  virtual ~SystemTrayBubble() {
    for (std::vector<ash::SystemTrayItem*>::iterator it = items_.begin();
        it != items_.end();
        ++it) {
      if (detailed_)
        (*it)->DestroyDetailedView();
      else
        (*it)->DestroyDefaultView();
    }
  }

  void set_can_activate(bool activate) { can_activate_ = activate; }

  void StartAutoCloseTimer(int seconds) {
    autoclose_.Stop();
    autoclose_delay_ = seconds;
    if (autoclose_delay_) {
      autoclose_.Start(FROM_HERE,
          base::TimeDelta::FromSeconds(autoclose_delay_),
          this, &SystemTrayBubble::AutoClose);
    }
  }

 private:
  void AutoClose() {
    GetWidget()->Close();
  }

  // Overridden from views::BubbleDelegateView.
  virtual void Init() OVERRIDE {
    SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical,
          1, 1, 1));
    set_background(new SystemTrayBubbleBackground(this));

    ash::SystemTrayDelegate* delegate =
        ash::Shell::GetInstance()->tray_delegate();
    ash::user::LoginStatus login_status = delegate->GetUserLoginStatus();
    for (std::vector<ash::SystemTrayItem*>::iterator it = items_.begin();
        it != items_.end();
        ++it) {
      views::View* view = detailed_ ? (*it)->CreateDetailedView(login_status) :
                                      (*it)->CreateDefaultView(login_status);
      if (view)
        AddChildView(new TrayItemContainer(view));
    }
  }

  virtual bool CanActivate() const OVERRIDE {
    return can_activate_;
  }

  virtual gfx::Size GetPreferredSize() OVERRIDE {
    gfx::Size size = views::BubbleDelegateView::GetPreferredSize();
    return gfx::Size(kTrayPopupWidth, size.height());
  }

  virtual void OnMouseEntered(const views::MouseEvent& event) OVERRIDE {
    autoclose_.Stop();
  }

  virtual void OnMouseExited(const views::MouseEvent& event) OVERRIDE {
    if (autoclose_delay_) {
      autoclose_.Stop();
      autoclose_.Start(FROM_HERE,
          base::TimeDelta::FromSeconds(autoclose_delay_),
          this, &SystemTrayBubble::AutoClose);
    }
  }

  ash::SystemTray* tray_;
  std::vector<ash::SystemTrayItem*> items_;
  bool detailed_;
  bool can_activate_;

  int autoclose_delay_;
  base::OneShotTimer<SystemTrayBubble> autoclose_;

  DISALLOW_COPY_AND_ASSIGN(SystemTrayBubble);
};

}  // namespace internal

NetworkIconInfo::NetworkIconInfo() {
}

NetworkIconInfo::~NetworkIconInfo() {
}

SystemTray::SystemTray()
    : items_(),
      audio_observer_(NULL),
      brightness_observer_(NULL),
      date_format_observer_(NULL),
      network_observer_(NULL),
      power_status_observer_(NULL),
      update_observer_(NULL),
      bubble_(NULL),
      popup_(NULL) {
  container_ = new views::View;
  container_->SetLayoutManager(new views::BoxLayout(
      views::BoxLayout::kHorizontal, 5, 0, kTrayPaddingBetweenItems));
  container_->set_background(new SystemTrayBackground);
  set_border(views::Border::CreateEmptyBorder(0, 0,
        kPaddingFromBottomOfScreen, kPaddingFromRightEdgeOfScreen));
  set_notify_enter_exit_on_child(true);
  set_focusable(true);
  SetLayoutManager(new views::BoxLayout(views::BoxLayout::kVertical, 0, 0, 0));
  AddChildView(container_);
}

SystemTray::~SystemTray() {
  if (popup_)
    popup_->CloseNow();
  for (std::vector<SystemTrayItem*>::iterator it = items_.begin();
      it != items_.end();
      ++it) {
    (*it)->DestroyTrayView();
  }
}

void SystemTray::AddTrayItem(SystemTrayItem* item) {
  items_.push_back(item);

  SystemTrayDelegate* delegate = Shell::GetInstance()->tray_delegate();
  views::View* tray_item = item->CreateTrayView(delegate->GetUserLoginStatus());
  if (tray_item) {
    container_->AddChildViewAt(tray_item, 0);
    PreferredSizeChanged();
  }
}

void SystemTray::RemoveTrayItem(SystemTrayItem* item) {
  NOTIMPLEMENTED();
}

void SystemTray::ShowDefaultView() {
  if (popup_) {
    popup_->RemoveObserver(this);
    popup_->Close();
  }
  popup_ = NULL;
  bubble_ = NULL;

  ShowItems(items_, false, true);
}

void SystemTray::ShowDetailedView(SystemTrayItem* item,
                                  int close_delay,
                                  bool activate) {
  if (popup_) {
    popup_->RemoveObserver(this);
    popup_->Close();
  }
  popup_ = NULL;
  bubble_ = NULL;

  std::vector<SystemTrayItem*> items;
  items.push_back(item);
  ShowItems(items, true, activate);
  bubble_->StartAutoCloseTimer(close_delay);
}

void SystemTray::UpdateAfterLoginStatusChange(user::LoginStatus login_status) {
  if (popup_)
    popup_->CloseNow();

  for (std::vector<SystemTrayItem*>::iterator it = items_.begin();
      it != items_.end();
      ++it) {
    (*it)->DestroyTrayView();
  }
  container_->RemoveAllChildViews(true);

  for (std::vector<SystemTrayItem*>::iterator it = items_.begin();
      it != items_.end();
      ++it) {
    views::View* view = (*it)->CreateTrayView(login_status);
    if (view)
      container_->AddChildViewAt(view, 0);
  }
  PreferredSizeChanged();
}

void SystemTray::ShowItems(std::vector<SystemTrayItem*>& items,
                           bool detailed,
                           bool activate) {
  CHECK(!popup_);
  CHECK(!bubble_);
  bubble_ = new internal::SystemTrayBubble(this, container_, items, detailed);
  bubble_->set_can_activate(activate);
  popup_ = views::BubbleDelegateView::CreateBubble(bubble_);
  bubble_->SetAlignment(views::BubbleBorder::ALIGN_EDGE_TO_ANCHOR_EDGE);
  popup_->non_client_view()->frame_view()->set_background(NULL);
  popup_->non_client_view()->frame_view()->set_border(
      new SystemTrayBubbleBorder(bubble_));
  popup_->AddObserver(this);

  // Setup animation.
  ash::SetWindowVisibilityAnimationType(popup_->GetNativeWindow(),
      ash::WINDOW_VISIBILITY_ANIMATION_TYPE_FADE);
  ash::SetWindowVisibilityAnimationTransition(popup_->GetNativeWindow(),
      ash::ANIMATE_BOTH);
  ash::SetWindowVisibilityAnimationDuration(popup_->GetNativeWindow(),
      base::TimeDelta::FromMilliseconds(kAnimationDurationForPopupMS));

  bubble_->Show();
}

bool SystemTray::OnKeyPressed(const views::KeyEvent& event) {
  if (event.key_code() == ui::VKEY_SPACE ||
      event.key_code() == ui::VKEY_RETURN) {
    if (popup_)
      popup_->Hide();
    else
      ShowItems(items_, false, true);
    return true;
  }
  return false;
}

bool SystemTray::OnMousePressed(const views::MouseEvent& event) {
  if (popup_)
    popup_->Hide();
  else
    ShowItems(items_, false, true);
  return true;
}

void SystemTray::OnMouseEntered(const views::MouseEvent& event) {
  static_cast<SystemTrayBackground*>(container_->background())->
      set_hovering(true);
  SchedulePaint();
}

void SystemTray::OnMouseExited(const views::MouseEvent& event) {
  static_cast<SystemTrayBackground*>(container_->background())->
      set_hovering(false);
  SchedulePaint();
}

void SystemTray::OnWidgetClosing(views::Widget* widget) {
  CHECK_EQ(popup_, widget);
  popup_ = NULL;
  bubble_ = NULL;
}

}  // namespace ash
