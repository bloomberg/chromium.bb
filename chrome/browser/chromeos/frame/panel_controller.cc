// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/frame/panel_controller.h"

#include <vector>

#include "app/resource_bundle.h"
#include "base/logging.h"
#include "base/scoped_ptr.h"
#include "base/singleton.h"
#include "base/string_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/wm_ipc.h"
#include "chrome/common/notification_service.h"
#include "gfx/canvas_skia.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/cros/chromeos_wm_ipc_enums.h"
#include "third_party/skia/include/effects/SkBlurMaskFilter.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "views/controls/button/image_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/event.h"
#include "views/painter.h"
#include "views/view.h"
#include "views/widget/widget_gtk.h"
#include "views/window/window.h"

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
const SkColor kTitleActiveColor = SK_ColorBLACK;
const SkColor kTitleInactiveColor = SK_ColorBLACK;
const SkColor kTitleCloseButtonColor = SK_ColorBLACK;

// Used to draw the background of the panel title window.
class TitleBackgroundPainter : public views::Painter {
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
    SkShader* s = SkGradientShader::CreateLinear(
        p, colors, NULL, 2, SkShader::kClamp_TileMode, NULL);
    paint.setShader(s);
    // Need to unref shader, otherwise never deleted.
    s->unref();
    canvas->AsCanvasSkia()->drawPath(path, paint);
  }
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
       client_event_handler_id_(0) {
}

void PanelController::Init(bool initial_focus,
                           const gfx::Rect& window_bounds,
                           XID creator_xid,
                           WmIpcPanelUserResizeType resize_type) {
  gfx::Rect title_bounds(0, 0, window_bounds.width(), kTitleHeight);

  title_window_ = new views::WidgetGtk(views::WidgetGtk::TYPE_WINDOW);
  title_window_->MakeTransparent();
  title_window_->Init(NULL, title_bounds);
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

  title_content_ = new TitleContentView(this);
  title_window_->SetContentsView(title_content_);
  UpdateTitleBar();
  title_window_->Show();
}

void PanelController::UpdateTitleBar() {
  if (!delegate_ || !title_window_)
    return;
  DCHECK(title_content_);
  title_content_->title_label()->SetText(
      UTF16ToWideHack(delegate_->GetPanelTitle()));
  title_content_->title_icon()->SetImage(delegate_->GetPanelIcon());
}

bool PanelController::TitleMousePressed(const views::MouseEvent& event) {
  if (!event.IsOnlyLeftMouseButton()) {
    return false;
  }
  GdkEvent* gdk_event = gtk_get_current_event();
  if (gdk_event->type != GDK_BUTTON_PRESS) {
    gdk_event_free(gdk_event);
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

  GdkEventButton last_button_event = gdk_event->button;
  mouse_down_ = true;
  mouse_down_abs_x_ = last_button_event.x_root;
  mouse_down_abs_y_ = last_button_event.y_root;
  mouse_down_offset_x_ = event.x() - title_width;
  mouse_down_offset_y_ = event.y();
  dragging_ = false;
  gdk_event_free(gdk_event);
  return true;
}

void PanelController::TitleMouseReleased(
    const views::MouseEvent& event, bool canceled) {
  if (!event.IsLeftMouseButton()) {
    return;
  }
  // Only handle clicks that started in our window.
  if (!mouse_down_) {
    return;
  }

  mouse_down_ = false;
  if (!dragging_) {
    SetState(expanded_ ?
             PanelController::MINIMIZED : PanelController::EXPANDED);
  } else {
    WmIpc::Message msg(WM_IPC_MESSAGE_WM_NOTIFY_PANEL_DRAG_COMPLETE);
    msg.set_param(0, panel_xid_);
    WmIpc::instance()->SendMessage(msg);
    dragging_ = false;
  }
}

void PanelController::SetState(State state) {
  WmIpc::Message msg(WM_IPC_MESSAGE_WM_SET_PANEL_STATE);
  msg.set_param(0, panel_xid_);
  msg.set_param(1, state == EXPANDED);
  WmIpc::instance()->SendMessage(msg);
}

bool PanelController::TitleMouseDragged(const views::MouseEvent& event) {
  if (!mouse_down_) {
    return false;
  }

  GdkEvent* gdk_event = gtk_get_current_event();
  if (gdk_event->type != GDK_MOTION_NOTIFY) {
    gdk_event_free(gdk_event);
    NOTREACHED();
    return false;
  }
  GdkEventMotion last_motion_event = gdk_event->motion;
  if (!dragging_) {
    if (views::View::ExceededDragThreshold(
        last_motion_event.x_root - mouse_down_abs_x_,
        last_motion_event.y_root - mouse_down_abs_y_)) {
      dragging_ = true;
    }
  }
  if (dragging_) {
    WmIpc::Message msg(WM_IPC_MESSAGE_WM_NOTIFY_PANEL_DRAGGED);
    msg.set_param(0, panel_xid_);
    msg.set_param(1, last_motion_event.x_root - mouse_down_offset_x_);
    msg.set_param(2, last_motion_event.y_root - mouse_down_offset_y_);
    WmIpc::instance()->SendMessage(msg);
  }
  gdk_event_free(gdk_event);
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
}

void PanelController::OnFocusOut() {
  if (title_window_)
    title_content_->OnFocusOut();
}

bool PanelController::PanelClientEvent(GdkEventClient* event) {
  WmIpc::Message msg;
  WmIpc::instance()->DecodeMessage(*event, &msg);
  if (msg.type() == WM_IPC_MESSAGE_CHROME_NOTIFY_PANEL_STATE) {
    bool new_state = msg.param(0);
    if (expanded_ != new_state) {
      expanded_ = new_state;
      State state = new_state ? EXPANDED : MINIMIZED;
      NotificationService::current()->Notify(
          NotificationType::PANEL_STATE_CHANGED,
          Source<PanelController>(this),
          Details<State>(&state));
    }
  }
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
  title_label_ = new views::Label(std::wstring());
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(title_label_);

  set_background(
      views::Background::CreateBackgroundPainter(
          true, new TitleBackgroundPainter()));
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
  DCHECK(panel_controller_) << "OnMousePressed after Close";
  return panel_controller_->TitleMousePressed(event);
}

void PanelController::TitleContentView::OnMouseReleased(
    const views::MouseEvent& event, bool canceled) {
  DCHECK(panel_controller_) << "MouseReleased after Close";
  return panel_controller_->TitleMouseReleased(event, canceled);
}

bool PanelController::TitleContentView::OnMouseDragged(
    const views::MouseEvent& event) {
  DCHECK(panel_controller_) << "MouseDragged after Close";
  return panel_controller_->TitleMouseDragged(event);
}

void PanelController::TitleContentView::OnFocusIn() {
  title_label_->SetColor(kTitleActiveColor);
  title_label_->SetFont(*active_font);
  Layout();
  SchedulePaint();
}

void PanelController::TitleContentView::OnFocusOut() {
  title_label_->SetColor(kTitleInactiveColor);
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
