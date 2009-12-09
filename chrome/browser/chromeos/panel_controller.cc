// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/panel_controller.h"

#include <vector>

#include "app/resource_bundle.h"
#include "base/logging.h"
#include "base/singleton.h"
#include "base/scoped_ptr.h"
#include "base/string_util.h"
#include "chrome/browser/browser.h"
#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/views/frame/browser_view.h"
#else
#include "chrome/browser/gtk/browser_window_gtk.h"
#endif
#include "chrome/browser/views/tabs/tab_overview_types.h"
#include "chrome/common/x11_util.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/controls/button/image_button.h"
#include "views/controls/image_view.h"
#include "views/controls/label.h"
#include "views/event.h"
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

const int kTitleWidth = 200;
const int kTitleHeight = 20;
const int kTitleIconSize = 16;
const int kTitleWidthPad = 2;
const int kTitleHeightPad = 1;
const int kButtonPad = 4;

const SkColor kActiveGradientStart = 0xffebeff9;
const SkColor kActiveGradientEnd = 0xffb3c4f6;
const SkColor kInactiveGradientStart = 0xfff2f2f2;
const SkColor kInactiveGradientEnd = 0xfff2f2f2;
const SkColor kActiveColor = SK_ColorBLACK;
const SkColor kInactiveColor = 0xff333333;
const SkColor kCloseButtonColor = SK_ColorBLACK;

static bool resources_initialized;
static void InitializeResources() {
  if (resources_initialized) {
    return;
  }

  resources_initialized = true;
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  inactive_font = new gfx::Font(rb.GetFont(ResourceBundle::BaseFont));
  active_font = new gfx::Font(inactive_font->DeriveFont(0, gfx::Font::BOLD));
  close_button_n = rb.GetBitmapNamed(IDR_TAB_CLOSE);
  close_button_m = rb.GetBitmapNamed(IDR_TAB_CLOSE_MASK);
  close_button_h = rb.GetBitmapNamed(IDR_TAB_CLOSE_H);
  close_button_p = rb.GetBitmapNamed(IDR_TAB_CLOSE_P);
  close_button_width = close_button_n->width();
  close_button_height = close_button_n->height();
}

}  // namespace

#if defined(TOOLKIT_VIEWS)
PanelController::PanelController(BrowserView* browser_window)
    :  browser_window_(browser_window),
       panel_(browser_window->GetNativeHandle()),
       panel_xid_(x11_util::GetX11WindowFromGtkWidget(GTK_WIDGET(panel_))),
       title_window_(NULL),
       expanded_(true),
       mouse_down_(false),
       dragging_(false) {
  Init(browser_window->bounds());
}
#else
PanelController::PanelController(BrowserWindowGtk* browser_window)
    :  browser_window_(browser_window),
       panel_(browser_window->window()),
       panel_xid_(x11_util::GetX11WindowFromGtkWidget(GTK_WIDGET(panel_))),
       title_window_(NULL),
       expanded_(true),
       mouse_down_(false),
       dragging_(false) {
  Init(browser_window->bounds());
}
#endif

void PanelController::Init(const gfx::Rect window_bounds) {
  gfx::Rect title_bounds(
      0, 0, window_bounds.width(), kTitleHeight);

  title_window_ = new views::WidgetGtk(views::WidgetGtk::TYPE_WINDOW);
  title_window_->Init(NULL, title_bounds);
  gtk_widget_set_size_request(title_window_->GetNativeView(),
                              title_bounds.width(), title_bounds.height());
  title_ = title_window_->GetNativeView();
  title_xid_ = x11_util::GetX11WindowFromGtkWidget(title_);

  TabOverviewTypes* tab_overview = TabOverviewTypes::instance();
  tab_overview->SetWindowType(
      title_,
      TabOverviewTypes::WINDOW_TYPE_CHROME_PANEL_TITLEBAR,
      NULL);
  std::vector<int> type_params;
  type_params.push_back(title_xid_);
  type_params.push_back(expanded_ ? 1 : 0);
  tab_overview->SetWindowType(
      GTK_WIDGET(panel_),
      TabOverviewTypes::WINDOW_TYPE_CHROME_PANEL,
      &type_params);

  g_signal_connect(
      panel_, "client-event", G_CALLBACK(OnPanelClientEvent), this);

  title_content_ = new TitleContentView(this);
  title_window_->SetContentsView(title_content_);
  title_window_->Show();
}

void PanelController::UpdateTitleBar() {
  if (!browser_window_ || !title_window_)
    return;
  Browser* browser = browser_window_->browser();
  title_content_->title_label()->SetText(
      UTF16ToWideHack(browser->GetWindowTitleForCurrentTab()));
  title_content_->title_icon()->SetImage(browser->GetCurrentPageIcon());
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
  GdkEventButton last_button_event = gdk_event->button;
  mouse_down_ = true;
  mouse_down_abs_x_ = last_button_event.x_root;
  mouse_down_abs_y_ = last_button_event.y_root;
  mouse_down_offset_x_ = event.x();
  mouse_down_offset_y_ = event.y();
  dragging_ = false;
  gdk_event_free(gdk_event);
  return true;
}

void PanelController::TitleMouseReleased(
    const views::MouseEvent& event, bool canceled) {
  if (!event.IsOnlyLeftMouseButton()) {
    return;
  }
  // Only handle clicks that started in our window.
  if (!mouse_down_) {
    return;
  }

  mouse_down_ = false;
  if (!dragging_) {
    TabOverviewTypes::Message msg(
        TabOverviewTypes::Message::WM_SET_PANEL_STATE);
    msg.set_param(0, panel_xid_);
    msg.set_param(1, expanded_ ? 0 : 1);
    TabOverviewTypes::instance()->SendMessage(msg);
  } else {
    TabOverviewTypes::Message msg(
        TabOverviewTypes::Message::WM_NOTIFY_PANEL_DRAG_COMPLETE);
    msg.set_param(0, panel_xid_);
    TabOverviewTypes::instance()->SendMessage(msg);
    dragging_ = false;
  }
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
    TabOverviewTypes::Message msg(TabOverviewTypes::Message::WM_MOVE_PANEL);
    msg.set_param(0, panel_xid_);
    msg.set_param(1, last_motion_event.x_root - mouse_down_offset_x_);
    msg.set_param(2, last_motion_event.y_root - mouse_down_offset_y_);
    TabOverviewTypes::instance()->SendMessage(msg);
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
  TabOverviewTypes::Message msg;
  TabOverviewTypes::instance()->DecodeMessage(*event, &msg);
  if (msg.type() == TabOverviewTypes::Message::CHROME_NOTIFY_PANEL_STATE) {
    expanded_ = msg.param(0);
  }
  return true;
}

void PanelController::Close() {
  // ignore if the title window is already closed.
  if (title_window_) {
    title_window_->Close();
    title_window_ = NULL;
  }
}

void PanelController::ButtonPressed(
    views::Button* sender, const views::Event& event) {
  if (title_window_ && sender == title_content_->close_button()) {
    browser_window_->Close();
    Close();
  }
}

PanelController::TitleContentView::TitleContentView(
    PanelController* panel_controller)
        : panel_controller_(panel_controller) {
  InitializeResources();
  close_button_ = new views::ImageButton(panel_controller_);
  close_button_->SetImage(views::CustomButton::BS_NORMAL, close_button_n);
  close_button_->SetImage(views::CustomButton::BS_HOT, close_button_h);
  close_button_->SetImage(views::CustomButton::BS_PUSHED, close_button_p);
  close_button_->SetBackground(
      kCloseButtonColor, close_button_n, close_button_m);
  AddChildView(close_button_);

  title_icon_ = new views::ImageView();
  AddChildView(title_icon_);
  title_label_ = new views::Label(std::wstring());
  title_label_->SetHorizontalAlignment(views::Label::ALIGN_LEFT);
  AddChildView(title_label_);

  // Default to inactive
  OnFocusOut();
}

void PanelController::TitleContentView::Layout() {
  int close_button_x = bounds().width() - (close_button_width + kButtonPad);
  close_button_->SetBounds(
      close_button_x,
      (bounds().height() - close_button_height) / 2,
      close_button_width,
      close_button_height);
  title_icon_->SetBounds(
      kTitleWidthPad,
      kTitleHeightPad * 2,
      kTitleIconSize,
      kTitleIconSize);
  int title_x = kTitleWidthPad * 2 + kTitleIconSize;
  title_label_->SetBounds(
      title_x,
      kTitleHeightPad,
      close_button_x - (title_x + kButtonPad),
      bounds().height() - kTitleHeightPad);
}

bool PanelController::TitleContentView::OnMousePressed(
    const views::MouseEvent& event) {
  return panel_controller_->TitleMousePressed(event);
}

void PanelController::TitleContentView::OnMouseReleased(
    const views::MouseEvent& event, bool canceled) {
  return panel_controller_->TitleMouseReleased(event, canceled);
}

bool PanelController::TitleContentView::OnMouseDragged(
    const views::MouseEvent& event) {
  return panel_controller_->TitleMouseDragged(event);
}

void PanelController::TitleContentView::OnFocusIn() {
  set_background(views::Background::CreateVerticalGradientBackground(
      kActiveGradientStart, kActiveGradientEnd));
  title_label_->SetColor(kActiveColor);
  title_label_->SetFont(*active_font);
  Layout();
  SchedulePaint();
}

void PanelController::TitleContentView::OnFocusOut() {
  set_background(views::Background::CreateVerticalGradientBackground(
      kInactiveGradientStart, kInactiveGradientEnd));
  title_label_->SetColor(kInactiveColor);
  title_label_->SetFont(*inactive_font);
  Layout();
  SchedulePaint();
}

}  // namespace chromeos
