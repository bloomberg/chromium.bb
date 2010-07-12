// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/custom_button.h"

#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "base/basictypes.h"
#include "chrome/browser/gtk/cairo_cached_surface.h"
#include "chrome/browser/gtk/gtk_chrome_button.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "gfx/gtk_util.h"
#include "gfx/skbitmap_operations.h"
#include "grit/theme_resources.h"
#include "third_party/skia/include/core/SkBitmap.h"

CustomDrawButtonBase::CustomDrawButtonBase(GtkThemeProvider* theme_provider,
    int normal_id, int active_id, int highlight_id, int depressed_id,
    int background_id)
    : background_image_(NULL),
      paint_override_(-1),
      normal_id_(normal_id),
      active_id_(active_id),
      highlight_id_(highlight_id),
      depressed_id_(depressed_id),
      button_background_id_(background_id),
      theme_provider_(theme_provider),
      flipped_(false) {
  for (int i = 0; i < (GTK_STATE_INSENSITIVE + 1); ++i)
    surfaces_[i].reset(new CairoCachedSurface);
  background_image_.reset(new CairoCachedSurface);

  if (theme_provider) {
    // Load images by pretending that we got a BROWSER_THEME_CHANGED
    // notification.
    theme_provider->InitThemesFor(this);

    registrar_.Add(this,
                   NotificationType::BROWSER_THEME_CHANGED,
                   NotificationService::AllSources());
  } else {
    // Load the button images from the resource bundle.
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    surfaces_[GTK_STATE_NORMAL]->UsePixbuf(
        normal_id ? rb.GetRTLEnabledPixbufNamed(normal_id) : NULL);
    surfaces_[GTK_STATE_ACTIVE]->UsePixbuf(
        active_id ? rb.GetRTLEnabledPixbufNamed(active_id) : NULL);
    surfaces_[GTK_STATE_PRELIGHT]->UsePixbuf(
        highlight_id ? rb.GetRTLEnabledPixbufNamed(highlight_id) : NULL);
    surfaces_[GTK_STATE_SELECTED]->UsePixbuf(NULL);
    surfaces_[GTK_STATE_INSENSITIVE]->UsePixbuf(
        depressed_id ? rb.GetRTLEnabledPixbufNamed(depressed_id) : NULL);
  }
}

CustomDrawButtonBase::~CustomDrawButtonBase() {
}

int CustomDrawButtonBase::Width() const {
  return surfaces_[0]->Width();
}

int CustomDrawButtonBase::Height() const {
  return surfaces_[0]->Height();
}

gboolean CustomDrawButtonBase::OnExpose(GtkWidget* widget,
                                        GdkEventExpose* e,
                                        gdouble hover_state) {
  int paint_state = paint_override_ >= 0 ?
                    paint_override_ : GTK_WIDGET_STATE(widget);

  // If the paint state is PRELIGHT then set it to NORMAL (we will paint the
  // hover state according to |hover_state_|).
  if (paint_state == GTK_STATE_PRELIGHT)
    paint_state = GTK_STATE_NORMAL;
  bool animating_hover = hover_state > 0.0 &&
      paint_state == GTK_STATE_NORMAL;
  CairoCachedSurface* pixbuf = PixbufForState(paint_state);
  CairoCachedSurface* hover_pixbuf = PixbufForState(GTK_STATE_PRELIGHT);

  if (!pixbuf || !pixbuf->valid())
    return FALSE;
  if (animating_hover && (!hover_pixbuf || !hover_pixbuf->valid()))
    return FALSE;

  cairo_t* cairo_context = gdk_cairo_create(GDK_DRAWABLE(widget->window));
  cairo_translate(cairo_context, widget->allocation.x, widget->allocation.y);

  if (flipped_) {
    // Horizontally flip the image for non-LTR/RTL reasons.
    cairo_translate(cairo_context, widget->allocation.width, 0.0f);
    cairo_scale(cairo_context, -1.0f, 1.0f);
  }

  // The widget might be larger than the pixbuf. Paint the pixbuf flush with the
  // start of the widget (left for LTR, right for RTL) and its bottom.
  gfx::Rect bounds = gfx::Rect(0, 0, pixbuf->Width(), 0);
  int x = gtk_util::MirroredLeftPointForRect(widget, bounds);
  int y = widget->allocation.height - pixbuf->Height();

  if (background_image_->valid()) {
    background_image_->SetSource(cairo_context, x, y);
    cairo_paint(cairo_context);
  }

  pixbuf->SetSource(cairo_context, x, y);
  cairo_paint(cairo_context);

  if (animating_hover) {
    hover_pixbuf->SetSource(cairo_context, x, y);
    cairo_paint_with_alpha(cairo_context, hover_state);
  }

  cairo_destroy(cairo_context);

  return TRUE;
}

void CustomDrawButtonBase::SetBackground(SkColor color,
                                         SkBitmap* image, SkBitmap* mask) {
  if (!image || !mask) {
    if (background_image_->valid()) {
      background_image_->UsePixbuf(NULL);
    }
  } else {
    SkBitmap img =
        SkBitmapOperations::CreateButtonBackground(color, *image, *mask);

    GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(&img);
    background_image_->UsePixbuf(pixbuf);
    g_object_unref(pixbuf);
  }
}

void CustomDrawButtonBase::Observe(NotificationType type,
    const NotificationSource& source, const NotificationDetails& details) {
  DCHECK(theme_provider_);
  DCHECK(NotificationType::BROWSER_THEME_CHANGED == type);

  surfaces_[GTK_STATE_NORMAL]->UsePixbuf(normal_id_ ?
      theme_provider_->GetRTLEnabledPixbufNamed(normal_id_) : NULL);
  surfaces_[GTK_STATE_ACTIVE]->UsePixbuf(active_id_ ?
      theme_provider_->GetRTLEnabledPixbufNamed(active_id_) : NULL);
  surfaces_[GTK_STATE_PRELIGHT]->UsePixbuf(highlight_id_ ?
      theme_provider_->GetRTLEnabledPixbufNamed(highlight_id_) : NULL);
  surfaces_[GTK_STATE_SELECTED]->UsePixbuf(NULL);
  surfaces_[GTK_STATE_INSENSITIVE]->UsePixbuf(depressed_id_ ?
      theme_provider_->GetRTLEnabledPixbufNamed(depressed_id_) : NULL);

  // Use the tinted background in some themes.
  if (button_background_id_) {
    SkColor color = theme_provider_->GetColor(
        BrowserThemeProvider::COLOR_BUTTON_BACKGROUND);
    SkBitmap* background = theme_provider_->GetBitmapNamed(
        IDR_THEME_BUTTON_BACKGROUND);
    SkBitmap* mask = theme_provider_->GetBitmapNamed(button_background_id_);

    SetBackground(color, background, mask);
  }
}

CairoCachedSurface* CustomDrawButtonBase::PixbufForState(int state) {
  CairoCachedSurface* pixbuf = surfaces_[state].get();

  // Fall back to the default image if we don't have one for this state.
  if (!pixbuf || !pixbuf->valid())
    pixbuf = surfaces_[GTK_STATE_NORMAL].get();

  return pixbuf;
}

// CustomDrawHoverController ---------------------------------------------------

CustomDrawHoverController::CustomDrawHoverController(GtkWidget* widget)
    : slide_animation_(this),
      widget_(NULL) {
  Init(widget);
}

CustomDrawHoverController::CustomDrawHoverController()
    : slide_animation_(this),
      widget_(NULL) {
}

CustomDrawHoverController::~CustomDrawHoverController() {
}

void CustomDrawHoverController::Init(GtkWidget* widget) {
  DCHECK(widget_ == NULL);
  widget_ = widget;
  g_signal_connect(widget_, "enter-notify-event",
                   G_CALLBACK(OnEnter), this);
  g_signal_connect(widget_, "leave-notify-event",
                   G_CALLBACK(OnLeave), this);
}

void CustomDrawHoverController::AnimationProgressed(
    const Animation* animation) {
  gtk_widget_queue_draw(widget_);
}

// static
gboolean CustomDrawHoverController::OnEnter(
    GtkWidget* widget,
    GdkEventCrossing* event,
    CustomDrawHoverController* controller) {
  controller->slide_animation_.Show();
  return FALSE;
}

// static
gboolean CustomDrawHoverController::OnLeave(
    GtkWidget* widget,
    GdkEventCrossing* event,
    CustomDrawHoverController* controller) {
  // When the user is holding a mouse button, we don't want to animate.
  if (event->state & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK))
    controller->slide_animation_.Reset();
  else
    controller->slide_animation_.Hide();
  return FALSE;
}

// CustomDrawButton ------------------------------------------------------------

CustomDrawButton::CustomDrawButton(int normal_id, int active_id,
    int highlight_id, int depressed_id)
    : button_base_(NULL, normal_id, active_id, highlight_id, depressed_id, 0),
      theme_provider_(NULL),
      gtk_stock_name_(NULL),
      icon_size_(GTK_ICON_SIZE_INVALID) {
  Init();

  // Initialize the theme stuff with no theme_provider.
  SetBrowserTheme();
}

CustomDrawButton::CustomDrawButton(GtkThemeProvider* theme_provider,
    int normal_id, int active_id, int highlight_id, int depressed_id,
    int background_id, const char* stock_id, GtkIconSize stock_size)
    : button_base_(theme_provider, normal_id, active_id, highlight_id,
                   depressed_id, background_id),
      theme_provider_(theme_provider),
      gtk_stock_name_(stock_id),
      icon_size_(stock_size) {
  Init();

  theme_provider_->InitThemesFor(this);
  registrar_.Add(this,
                 NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
}

CustomDrawButton::~CustomDrawButton() {
  widget_.Destroy();
}

void CustomDrawButton::Init() {
  widget_.Own(gtk_chrome_button_new());
  GTK_WIDGET_UNSET_FLAGS(widget(), GTK_CAN_FOCUS);
  g_signal_connect(widget(), "expose-event",
                   G_CALLBACK(OnCustomExpose), this);
  hover_controller_.Init(widget());
}

void CustomDrawButton::Observe(NotificationType type,
    const NotificationSource& source, const NotificationDetails& details) {
  DCHECK(NotificationType::BROWSER_THEME_CHANGED == type);
  SetBrowserTheme();
}

void CustomDrawButton::SetPaintOverride(GtkStateType state) {
  button_base_.set_paint_override(state);
  gtk_chrome_button_set_paint_state(GTK_CHROME_BUTTON(widget()), state);
  gtk_widget_queue_draw(widget());
}

void CustomDrawButton::UnsetPaintOverride() {
  button_base_.set_paint_override(-1);
  gtk_chrome_button_unset_paint_state(GTK_CHROME_BUTTON(widget()));
  gtk_widget_queue_draw(widget());
}

void CustomDrawButton::SetBackground(SkColor color,
                                     SkBitmap* image, SkBitmap* mask) {
  button_base_.SetBackground(color, image, mask);
}

// static
gboolean CustomDrawButton::OnCustomExpose(GtkWidget* widget,
                                          GdkEventExpose* e,
                                          CustomDrawButton* button) {
  if (button->theme_provider_ && button->theme_provider_->UseGtkTheme()) {
    // Continue processing this expose event.
    return FALSE;
  } else {
    double hover_state = button->hover_controller_.GetCurrentValue();
    return button->button_base_.OnExpose(widget, e, hover_state);
  }
}

// static
CustomDrawButton* CustomDrawButton::CloseButton(
    GtkThemeProvider* theme_provider) {
  CustomDrawButton* button = new CustomDrawButton(
      theme_provider, IDR_CLOSE_BAR, IDR_CLOSE_BAR_P,
      IDR_CLOSE_BAR_H, 0, 0, GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
  return button;
}

void CustomDrawButton::SetBrowserTheme() {
  bool use_gtk = theme_provider_ && theme_provider_->UseGtkTheme();

  if (use_gtk && gtk_stock_name_) {
    gtk_button_set_image(
        GTK_BUTTON(widget()),
        gtk_image_new_from_stock(gtk_stock_name_, icon_size_));
    gtk_widget_set_size_request(widget(), -1, -1);
    gtk_widget_set_app_paintable(widget(), FALSE);
  } else {
    gtk_widget_set_size_request(widget(), button_base_.Width(),
                                button_base_.Height());

    gtk_widget_set_app_paintable(widget(), TRUE);
  }

  gtk_chrome_button_set_use_gtk_rendering(
      GTK_CHROME_BUTTON(widget()), use_gtk);
}
