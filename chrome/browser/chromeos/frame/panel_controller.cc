// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/frame/panel_controller.h"

#if defined(TOUCH_UI)
#include <X11/Xlib.h>
#include <X11/extensions/XInput2.h>
#endif

#include <vector>

#include "base/logging.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/singleton.h"
#include "base/string_util.h"
#include "base/time.h"
#include "base/utf_string_conversions.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_service.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "grit/theme_resources_standard.h"
#include "grit/ui_resources.h"
#include "third_party/cros_system_api/window_manager/chromeos_wm_ipc_enums.h"
#include "third_party/skia/include/effects/SkBlurMaskFilter.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/views/controls/button/image_button.h"
#include "ui/views/controls/image_view.h"
#include "ui/views/controls/label.h"
#include "ui/views/events/event.h"
#include "ui/views/widget/widget.h"
#include "views/painter.h"
#include "views/view.h"

#if defined(TOOLKIT_USES_GTK)
#include "chrome/browser/chromeos/legacy_window_manager/wm_ipc.h"
#endif

#if defined(TOUCH_UI)
namespace {

gfx::Point RootLocationFromXEvent(const XEvent* xev) {
  switch (xev->type) {
    case ButtonPress:
    case ButtonRelease:
      return gfx::Point(xev->xbutton.x_root, xev->xbutton.y_root);
    case MotionNotify:
      return gfx::Point(xev->xmotion.x_root, xev->xmotion.y_root);

    case GenericEvent: {
      const XIDeviceEvent* xiev =
          static_cast<XIDeviceEvent*>(xev->xcookie.data);
      switch (xiev->evtype) {
        case XI_ButtonPress:
        case XI_ButtonRelease:
        case XI_Motion:
          return gfx::Point(static_cast<int>(xiev->root_x),
                            static_cast<int>(xiev->root_y));
      }
    }

    default:
      NOTREACHED();
  }

  return gfx::Point();
}

}  // namespace
#endif  // defined(TOUCH_UI)

namespace chromeos {

static int close_button_width;
static int close_button_height;
static SkBitmap* close_button_n;
static SkBitmap* close_button_m;
static SkBitmap* close_button_h;
static SkBitmap* close_button_p;
static gfx::Font* active_font = NULL;
static gfx::Font* inactive_font = NULL;

namespace {

const int kTitleHeight = 24;
const int kTitleIconSize = 16;
const int kTitleWidthPad = 4;
const int kTitleHeightPad = 4;
const int kTitleCornerRadius = 4;
const int kTitleCloseButtonPad = 6;
const SkColor kTitleActiveGradientStart = SK_ColorWHITE;
const SkColor kTitleActiveGradientEnd = 0xffe7edf1;
const SkColor kTitleUrgentGradientStart = 0xfffea044;
const SkColor kTitleUrgentGradientEnd = 0xfffa983a;
const SkColor kTitleActiveTextColor = SK_ColorBLACK;
const SkColor kTitleInactiveTextColor = SK_ColorBLACK;
const SkColor kTitleUrgentTextColor = SK_ColorWHITE;
const SkColor kTitleCloseButtonColor = SK_ColorBLACK;
// Delay before the urgency can be set after it has been cleared.
const base::TimeDelta kSetUrgentDelay = base::TimeDelta::FromMilliseconds(500);

// Used to draw the background of the panel title window.
class TitleBackgroundPainter : public views::Painter {
 public:
  explicit TitleBackgroundPainter(PanelController* controller)
      : panel_controller_(controller) { }

 private:
  virtual void Paint(int w, int h, gfx::Canvas* canvas) {
    SkRect rect = {0, 0, w, h};
    SkPath path;
    SkScalar corners[] = {
        kTitleCornerRadius, kTitleCornerRadius,
        kTitleCornerRadius, kTitleCornerRadius,
        0, 0,
        0, 0
    };
    path.addRoundRect(rect, corners);
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setFlags(SkPaint::kAntiAlias_Flag);
    SkPoint p[2] = { {0, 0}, {0, h} };
    SkColor colors[2] = {kTitleActiveGradientStart, kTitleActiveGradientEnd};
    if (panel_controller_->urgent()) {
      colors[0] = kTitleUrgentGradientStart;
      colors[1] = kTitleUrgentGradientEnd;
    }
    SkShader* s = SkGradientShader::CreateLinear(
        p, colors, NULL, 2, SkShader::kClamp_TileMode, NULL);
    paint.setShader(s);
    // Need to unref shader, otherwise never deleted.
    s->unref();
    canvas->GetSkCanvas()->drawPath(path, paint);
  }

  PanelController* panel_controller_;
};

static bool resources_initialized;
static void InitializeResources() {
  if (resources_initialized) {
    return;
  }

  resources_initialized = true;
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  gfx::Font base_font = rb.GetFont(ResourceBundle::BaseFont);
  // Title fonts are the same for active and inactive.
  inactive_font = new gfx::Font(base_font.DeriveFont(0, gfx::Font::BOLD));
  active_font = inactive_font;
  close_button_n = rb.GetBitmapNamed(IDR_TAB_CLOSE);
  close_button_m = rb.GetBitmapNamed(IDR_TAB_CLOSE_MASK);
  close_button_h = rb.GetBitmapNamed(IDR_TAB_CLOSE_H);
  close_button_p = rb.GetBitmapNamed(IDR_TAB_CLOSE_P);
  close_button_width = close_button_n->width();
  close_button_height = close_button_n->height();
}

}  // namespace

PanelController::PanelController(Delegate* delegate,
                                 GtkWindow* window)
    :  delegate_(delegate),
       panel_(window),
       panel_xid_(ui::GetX11WindowFromGtkWidget(GTK_WIDGET(panel_))),
       title_window_(NULL),
       title_(NULL),
       title_content_(NULL),
       expanded_(true),
       mouse_down_(false),
       dragging_(false),
       client_event_handler_id_(0),
       focused_(false),
       urgent_(false) {
}

void PanelController::Init(bool initial_focus,
                           const gfx::Rect& window_bounds,
                           XID creator_xid,
                           WmIpcPanelUserResizeType resize_type) {
  gfx::Rect title_bounds(0, 0, window_bounds.width(), kTitleHeight);

  title_window_ = new views::Widget;
  views::Widget::InitParams params(
      views::Widget::InitParams::TYPE_WINDOW_FRAMELESS);
  params.transparent = true;
  params.bounds = title_bounds;
  title_window_->Init(params);

#if defined(TOOLKIT_USES_GTK)
  gtk_widget_set_size_request(title_window_->GetNativeView(),
                              title_bounds.width(), title_bounds.height());
  title_ = title_window_->GetNativeView();
  title_xid_ = ui::GetX11WindowFromGtkWidget(title_);

  WmIpc::instance()->SetWindowType(
      title_,
      WM_IPC_WINDOW_CHROME_PANEL_TITLEBAR,
      NULL);
  std::vector<int> type_params;
  type_params.push_back(title_xid_);
  type_params.push_back(expanded_ ? 1 : 0);
  type_params.push_back(initial_focus ? 1 : 0);
  type_params.push_back(creator_xid);
  type_params.push_back(resize_type);
  WmIpc::instance()->SetWindowType(
      GTK_WIDGET(panel_),
      WM_IPC_WINDOW_CHROME_PANEL_CONTENT,
      &type_params);

  client_event_handler_id_ = g_signal_connect(
      panel_, "client-event", G_CALLBACK(OnPanelClientEvent), this);
#endif

  title_content_ = new TitleContentView(this);
  title_window_->SetContentsView(title_content_);
  UpdateTitleBar();
  title_window_->Show();
}

void PanelController::UpdateTitleBar() {
  if (!delegate_ || !title_window_)
    return;
  title_content_->title_label()->SetText(delegate_->GetPanelTitle());
  title_content_->title_icon()->SetImage(delegate_->GetPanelIcon());
}

void PanelController::SetUrgent(bool urgent) {
  if (!urgent)
    urgent_cleared_time_ = base::TimeTicks::Now();
  if (urgent == urgent_)
    return;
  if (urgent && focused_)
    return;  // Don't set urgency for focused panels.
  if (urgent && base::TimeTicks::Now() < urgent_cleared_time_ + kSetUrgentDelay)
    return;  // Don't set urgency immediately after clearing it.
  urgent_ = urgent;
  if (title_window_) {
    gtk_window_set_urgency_hint(panel_, urgent ? TRUE : FALSE);
    title_content_->title_label()->SetDisabledColor(urgent ?
        kTitleUrgentTextColor : kTitleInactiveTextColor);
    title_content_->SchedulePaint();
  }
}

bool PanelController::TitleMousePressed(const views::MouseEvent& event) {
  if (!event.IsOnlyLeftMouseButton())
    return false;
  if (event.type() != ui::ET_MOUSE_PRESSED) {
    NOTREACHED();
    return false;
  }
  DCHECK(title_);
  // Get the last titlebar width that we saw in a ConfigureNotify event -- we
  // need to give drag positions in terms of the top-right corner of the
  // titlebar window.  See WM_IPC_MESSAGE_WM_NOTIFY_PANEL_DRAGGED's declaration
  // for details.
  gint title_width = 1;
  gtk_window_get_size(GTK_WINDOW(title_), &title_width, NULL);

  mouse_down_ = true;
  mouse_down_offset_x_ = event.x() - title_width;
  mouse_down_offset_y_ = event.y();
  dragging_ = false;

#if defined(TOOLKIT_USES_GTK)
  const GdkEvent* gdk_event = event.gdk_event();
  GdkEventButton last_button_event = gdk_event->button;
  mouse_down_abs_x_ = last_button_event.x_root;
  mouse_down_abs_y_ = last_button_event.y_root;
#else
  const XEvent* xev = event.native_event();
  gfx::Point abs_location = RootLocationFromXEvent(xev);
  mouse_down_abs_x_ = abs_location.x();
  mouse_down_abs_y_ = abs_location.y();
#endif
  return true;
}

void PanelController::TitleMouseReleased(const views::MouseEvent& event) {
  if (event.IsLeftMouseButton())
    TitleMouseCaptureLost();
}

void PanelController::TitleMouseCaptureLost() {
  // Only handle clicks that started in our window.
  if (!mouse_down_)
    return;

  mouse_down_ = false;
  if (!dragging_) {
    if (expanded_) {
      // Always activate the panel here, even if we are about to minimize it.
      // This lets panels like GTalk know that they have been acknowledged, so
      // they don't change the title again (which would trigger SetUrgent).
      // Activating the panel also clears the urgent state.
      delegate_->ActivatePanel();
      SetState(PanelController::MINIMIZED);
    } else {
      // If we're expanding the panel, do so before focusing it.  This lets the
      // window manager know that the panel is being expanded in response to a
      // user action; see http://crosbug.com/14735.
      SetState(PanelController::EXPANDED);
      delegate_->ActivatePanel();
    }
  } else {
#if defined(TOOLKIT_USES_GTK)
    WmIpc::Message msg(WM_IPC_MESSAGE_WM_NOTIFY_PANEL_DRAG_COMPLETE);
    msg.set_param(0, panel_xid_);
    WmIpc::instance()->SendMessage(msg);
#endif
    dragging_ = false;
  }
}

void PanelController::SetState(State state) {
#if defined(TOOLKIT_USES_GTK)
  WmIpc::Message msg(WM_IPC_MESSAGE_WM_SET_PANEL_STATE);
  msg.set_param(0, panel_xid_);
  msg.set_param(1, state == EXPANDED);
  WmIpc::instance()->SendMessage(msg);
#endif
}

bool PanelController::TitleMouseDragged(const views::MouseEvent& event) {
  if (!mouse_down_)
    return false;
  if (event.type() != ui::ET_MOUSE_MOVED &&
      event.type() != ui::ET_MOUSE_DRAGGED) {
    NOTREACHED();
    return false;
  }

#if !defined(TOUCH_UI)
  const GdkEvent* gdk_event = event.gdk_event();
  GdkEventMotion last_motion_event = gdk_event->motion;
  int x_root = last_motion_event.x_root;
  int y_root = last_motion_event.y_root;
#else
  const XEvent* xev = event.native_event();
  gfx::Point abs_location = RootLocationFromXEvent(xev);
  int x_root = abs_location.x();
  int y_root = abs_location.y();
#endif

  if (!dragging_) {
    if (views::View::ExceededDragThreshold(x_root - mouse_down_abs_x_,
                                           y_root - mouse_down_abs_y_)) {
      dragging_ = true;
    }
  }
#if defined(TOOLKIT_USES_GTK)
  if (dragging_) {
    WmIpc::Message msg(WM_IPC_MESSAGE_WM_NOTIFY_PANEL_DRAGGED);
    msg.set_param(0, panel_xid_);
    msg.set_param(1, x_root - mouse_down_offset_x_);
    msg.set_param(2, y_root - mouse_down_offset_y_);
    WmIpc::instance()->SendMessage(msg);
  }
#endif
  return true;
}

// static
bool PanelController::OnPanelClientEvent(
    GtkWidget* widget,
    GdkEventClient* event,
    PanelController* panel_controller) {
  return panel_controller->PanelClientEvent(event);
}

void PanelController::OnFocusIn() {
  if (title_window_)
    title_content_->OnFocusIn();
  focused_ = true;
  // Clear urgent when focused.
  SetUrgent(false);
}

void PanelController::OnFocusOut() {
  focused_ = false;
  if (title_window_)
    title_content_->OnFocusOut();
}

bool PanelController::PanelClientEvent(GdkEventClient* event) {
#if defined(TOOLKIT_USES_GTK)
  WmIpc::Message msg;
  WmIpc::instance()->DecodeMessage(*event, &msg);
  if (msg.type() == WM_IPC_MESSAGE_CHROME_NOTIFY_PANEL_STATE) {
    bool new_state = msg.param(0);
    if (expanded_ != new_state) {
      expanded_ = new_state;
      State state = new_state ? EXPANDED : MINIMIZED;
      content::NotificationService::current()->Notify(
          chrome::NOTIFICATION_PANEL_STATE_CHANGED,
          content::Source<PanelController>(this),
          content::Details<State>(&state));
    }
  }
#endif
  return true;
}

void PanelController::Close() {
  if (client_event_handler_id_ > 0) {
    g_signal_handler_disconnect(panel_, client_event_handler_id_);
    client_event_handler_id_ = 0;
  }
  // ignore if the title window is already closed.
  if (title_window_) {
    title_window_->Close();
    title_window_ = NULL;
    title_ = NULL;
    title_content_->OnClose();
    title_content_ = NULL;
  }
}

void PanelController::OnCloseButtonPressed() {
  DCHECK(title_content_);
  if (title_window_) {
    if (delegate_) {
      if (!delegate_->CanClosePanel())
        return;
      delegate_->ClosePanel();
    }
    Close();
  }
}

PanelController::TitleContentView::TitleContentView(
    PanelController* panel_controller)
        : panel_controller_(panel_controller) {
  VLOG(1) << "panel: c " << this;
  InitializeResources();
  close_button_ = new views::ImageButton(this);
  close_button_->SetImage(views::CustomButton::BS_NORMAL, close_button_n);
  close_button_->SetImage(views::CustomButton::BS_HOT, close_button_h);
  close_button_->SetImage(views::CustomButton::BS_PUSHED, close_button_p);
  close_button_->SetBackground(
      kTitleCloseButtonColor, close_button_n, close_button_m);
  AddChildView(close_button_);

  title_icon_ = new views::ImageView();
  AddChildView(title_icon_);
  title_label_ = new views::Label(string16());
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  title_label_->SetAutoColorReadabilityEnabled(false);
  title_label_->SetEnabledColor(kTitleActiveTextColor);
  title_label_->SetDisabledColor(kTitleInactiveTextColor);
  title_label_->SetEnabled(false);
  AddChildView(title_label_);

  set_background(
      views::Background::CreateBackgroundPainter(
          true, new TitleBackgroundPainter(panel_controller)));
  OnFocusOut();
}

void PanelController::TitleContentView::Layout() {
  int close_button_x = bounds().width() -
      (close_button_width + kTitleCloseButtonPad);
  close_button_->SetBounds(
      close_button_x,
      (bounds().height() - close_button_height) / 2,
      close_button_width,
      close_button_height);
  title_icon_->SetBounds(
      kTitleWidthPad,
      kTitleHeightPad,
      kTitleIconSize,
      kTitleIconSize);
  int title_x = kTitleWidthPad * 2 + kTitleIconSize;
  title_label_->SetBounds(
      title_x,
      0,
      close_button_x - (title_x + kTitleCloseButtonPad),
      bounds().height());
}

bool PanelController::TitleContentView::OnMousePressed(
    const views::MouseEvent& event) {
  return panel_controller_->TitleMousePressed(event);
}

void PanelController::TitleContentView::OnMouseReleased(
    const views::MouseEvent& event) {
  panel_controller_->TitleMouseReleased(event);
}

void PanelController::TitleContentView::OnMouseCaptureLost() {
  panel_controller_->TitleMouseCaptureLost();
}

bool PanelController::TitleContentView::OnMouseDragged(
    const views::MouseEvent& event) {
  return panel_controller_->TitleMouseDragged(event);
}

void PanelController::TitleContentView::OnFocusIn() {
  title_label_->SetEnabled(true);
  title_label_->SetFont(*active_font);
  Layout();
  SchedulePaint();
}

void PanelController::TitleContentView::OnFocusOut() {
  title_label_->SetEnabled(false);
  title_label_->SetFont(*inactive_font);
  Layout();
  SchedulePaint();
}

void PanelController::TitleContentView::OnClose() {
  panel_controller_ = NULL;
}

void PanelController::TitleContentView::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (panel_controller_ && sender == close_button_)
    panel_controller_->OnCloseButtonPressed();
}

PanelController::TitleContentView::~TitleContentView() {
  VLOG(1) << "panel: delete " << this;
}

}  // namespace chromeos
