// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/login/helper.h"

#include "base/file_util.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/cros/network_library.h"
#include "chrome/browser/google/google_util.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/effects/SkGradientShader.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/canvas_skia.h"
#include "ui/gfx/screen.h"
#include "ui/views/controls/button/menu_button.h"
#include "ui/views/controls/label.h"
#include "ui/views/controls/textfield/textfield.h"
#include "ui/views/controls/throbber.h"
#include "ui/views/painter.h"
#include "ui/views/widget/widget.h"

namespace chromeos {

namespace {

// Time in ms per throbber frame.
const int kThrobberFrameMs = 60;

// Time in ms before smoothed throbber is shown.
const int kThrobberStartDelayMs = 500;

const SkColor kBackgroundCenterColor = SkColorSetRGB(41, 50, 67);
const SkColor kBackgroundEdgeColor = SK_ColorBLACK;

const char kAccountRecoveryHelpUrl[] =
    "https://www.google.com/support/accounts/bin/answer.py?answer=48598";

class BackgroundPainter : public views::Painter {
 public:
  BackgroundPainter() {}

  virtual void Paint(int w, int h, gfx::Canvas* canvas) {
    SkRect rect;
    rect.set(SkIntToScalar(0),
             SkIntToScalar(0),
             SkIntToScalar(w),
             SkIntToScalar(h));
    SkPaint paint;
    paint.setStyle(SkPaint::kFill_Style);
    paint.setFlags(SkPaint::kAntiAlias_Flag);
    SkColor colors[2] = { kBackgroundCenterColor, kBackgroundEdgeColor };
    SkShader* s = SkGradientShader::CreateRadial(
        SkPoint::Make(SkIntToScalar(w / 2), SkIntToScalar(h / 2)),
        SkIntToScalar(w / 2),
        colors,
        NULL,
        2,
        SkShader::kClamp_TileMode,
        NULL);
    paint.setShader(s);
    s->unref();
    canvas->GetSkCanvas()->drawRect(rect, paint);
  }

 private:
  DISALLOW_COPY_AND_ASSIGN(BackgroundPainter);
};

}  // namespace

ThrobberHostView::ThrobberHostView()
    : host_view_(this),
      throbber_widget_(NULL) {
}

ThrobberHostView::~ThrobberHostView() {
  StopThrobber();
}

void ThrobberHostView::StartThrobber() {
#if defined(TOOLKIT_USES_GTK)
  StopThrobber();

  views::Widget* host_widget = host_view_->GetWidget();
  if (!host_widget) {
    LOG(WARNING) << "Failed to start the throbber: no Widget";
    return;
  }

  GtkWidget* host_gtk_window = host_widget->GetNativeView();
  while (host_gtk_window && !GTK_IS_WINDOW(host_gtk_window))
    host_gtk_window = gtk_widget_get_parent(host_gtk_window);
  if (!host_gtk_window) {
    LOG(WARNING) << "Failed to start the throbber: no GtkWindow";
    return;
  }

  views::SmoothedThrobber* throbber = CreateDefaultSmoothedThrobber();
  throbber->set_stop_delay_ms(0);
  gfx::Rect throbber_bounds = CalculateThrobberBounds(throbber);
  throbber_bounds.Offset(host_view_->GetScreenBounds().origin());

  throbber_widget_ = new views::Widget;

  views::Widget::InitParams params(views::Widget::InitParams::TYPE_POPUP);
  params.transparent = true;
  params.bounds = throbber_bounds;
  params.parent = host_gtk_window;
  throbber_widget_->Init(params);
  throbber_widget_->SetContentsView(throbber);
  // This keeps the window from flashing at startup.
  gdk_window_set_back_pixmap(
      throbber_widget_->GetNativeView()->window, NULL, false);
  throbber_widget_->Show();
  // WM can ignore bounds before widget is shown.
  throbber_widget_->SetBounds(throbber_bounds);
  throbber->Start();
#else
  // TODO(saintlou): Do we need a throbber for Aura?
  NOTIMPLEMENTED();
#endif
}

void ThrobberHostView::StopThrobber() {
  if (throbber_widget_) {
    throbber_widget_->Close();
    throbber_widget_ = NULL;
  }
}

gfx::Rect ThrobberHostView::CalculateThrobberBounds(views::Throbber* throbber) {
  gfx::Rect bounds(throbber->GetPreferredSize());
  bounds.set_x(
      host_view_->width() - login::kThrobberRightMargin - bounds.width());
  bounds.set_y((host_view_->height() - bounds.height()) / 2);
  return bounds;
}

views::SmoothedThrobber* CreateDefaultSmoothedThrobber() {
  views::SmoothedThrobber* throbber =
      new views::SmoothedThrobber(kThrobberFrameMs);
  throbber->SetFrames(
      ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_SPINNER));
  throbber->set_start_delay_ms(kThrobberStartDelayMs);
  return throbber;
}

views::Throbber* CreateDefaultThrobber() {
  views::Throbber* throbber = new views::Throbber(kThrobberFrameMs, false);
  throbber->SetFrames(
      ResourceBundle::GetSharedInstance().GetBitmapNamed(IDR_SPINNER));
  return throbber;
}

views::Painter* CreateBackgroundPainter() {
  return new BackgroundPainter();
}

gfx::Rect CalculateScreenBounds(const gfx::Size& size) {
  gfx::Rect bounds(gfx::Screen::GetMonitorAreaNearestWindow(NULL));
  if (!size.IsEmpty()) {
    int horizontal_diff = bounds.width() - size.width();
    int vertical_diff = bounds.height() - size.height();
    bounds.Inset(horizontal_diff / 2, vertical_diff / 2);
  }
  return bounds;
}

void CorrectLabelFontSize(views::Label* label) {
  if (label)
    label->SetFont(label->font().DeriveFont(kFontSizeCorrectionDelta));
}

void CorrectMenuButtonFontSize(views::MenuButton* button) {
  if (button)
    button->SetFont(button->font().DeriveFont(kFontSizeCorrectionDelta));
}

void CorrectNativeButtonFontSize(views::NativeTextButton* button) {
  if (button)
    button->SetFont(button->font().DeriveFont(kFontSizeCorrectionDelta));
}

void CorrectTextfieldFontSize(views::Textfield* textfield) {
  if (textfield)
    textfield->SetFont(textfield->font().DeriveFont(kFontSizeCorrectionDelta));
}

void SetAndCorrectTextfieldFont(views::Textfield* textfield,
                                const gfx::Font& font) {
  if (textfield)
    textfield->SetFont(font.DeriveFont(kFontSizeCorrectionDelta));
}

GURL GetAccountRecoveryHelpUrl() {
  return google_util::AppendGoogleLocaleParam(GURL(kAccountRecoveryHelpUrl));
}

string16 GetCurrentNetworkName(NetworkLibrary* network_library) {
  if (!network_library)
    return string16();

  if (network_library->ethernet_connected()) {
    return l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
  } else if (network_library->wifi_connected()) {
    return UTF8ToUTF16(network_library->wifi_network()->name());
  } else if (network_library->cellular_connected()) {
    return UTF8ToUTF16(network_library->cellular_network()->name());
  } else if (network_library->ethernet_connecting()) {
    return l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
  } else if (network_library->wifi_connecting()) {
    return UTF8ToUTF16(network_library->wifi_network()->name());
  } else if (network_library->cellular_connecting()) {
    return UTF8ToUTF16(network_library->cellular_network()->name());
  } else {
    return string16();
  }
}

namespace login {

gfx::Size WideButton::GetPreferredSize() {
  gfx::Size preferred_size = NativeTextButton::GetPreferredSize();
  // Set minimal width.
  if (preferred_size.width() < kButtonMinWidth)
    preferred_size.set_width(kButtonMinWidth);
  return preferred_size;
}

}  // namespace login

}  // namespace chromeos
