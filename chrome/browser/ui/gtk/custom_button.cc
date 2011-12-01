// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/custom_button.h"

#include "base/basictypes.h"
#include "base/logging.h"
#include "chrome/browser/ui/gtk/cairo_cached_surface.h"
#include "chrome/browser/ui/gtk/gtk_chrome_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_source.h"
#include "grit/theme_resources_standard.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/skbitmap_operations.h"

CustomDrawButtonBase::CustomDrawButtonBase(GtkThemeService* theme_provider,
                                           int normal_id,
                                           int pressed_id,
                                           int hover_id,
                                           int disabled_id)
    : background_image_(NULL),
      paint_override_(-1),
      normal_id_(normal_id),
      pressed_id_(pressed_id),
      hover_id_(hover_id),
      disabled_id_(disabled_id),
      theme_service_(theme_provider),
      flipped_(false) {
  for (int i = 0; i < (GTK_STATE_INSENSITIVE + 1); ++i)
    surfaces_[i].reset(new CairoCachedSurface);
  background_image_.reset(new CairoCachedSurface);

  if (theme_provider) {
    // Load images by pretending that we got a BROWSER_THEME_CHANGED
    // notification.
    theme_provider->InitThemesFor(this);

    registrar_.Add(this,
                   chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                   content::Source<ThemeService>(theme_provider));
  } else {
    // Load the button images from the resource bundle.
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    surfaces_[GTK_STATE_NORMAL]->UsePixbuf(
        normal_id_ ? rb.GetRTLEnabledPixbufNamed(normal_id_) : NULL);
    surfaces_[GTK_STATE_ACTIVE]->UsePixbuf(
        pressed_id_ ? rb.GetRTLEnabledPixbufNamed(pressed_id_) : NULL);
    surfaces_[GTK_STATE_PRELIGHT]->UsePixbuf(
        hover_id_ ? rb.GetRTLEnabledPixbufNamed(hover_id_) : NULL);
    surfaces_[GTK_STATE_SELECTED]->UsePixbuf(NULL);
    surfaces_[GTK_STATE_INSENSITIVE]->UsePixbuf(
        disabled_id_ ? rb.GetRTLEnabledPixbufNamed(disabled_id_) : NULL);
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
                    paint_override_ : gtk_widget_get_state(widget);

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
  GtkAllocation allocation;
  gtk_widget_get_allocation(widget, &allocation);
  cairo_translate(cairo_context, allocation.x, allocation.y);

  if (flipped_) {
    // Horizontally flip the image for non-LTR/RTL reasons.
    cairo_translate(cairo_context, allocation.width, 0.0f);
    cairo_scale(cairo_context, -1.0f, 1.0f);
  }

  // The widget might be larger than the pixbuf. Paint the pixbuf flush with the
  // start of the widget (left for LTR, right for RTL) and its bottom.
  gfx::Rect bounds = gfx::Rect(0, 0, pixbuf->Width(), 0);
  int x = gtk_util::MirroredLeftPointForRect(widget, bounds);
  int y = allocation.height - pixbuf->Height();

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

  GtkWidget* child = gtk_bin_get_child(GTK_BIN(widget));
  if (child)
    gtk_container_propagate_expose(GTK_CONTAINER(widget), child, e);

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

void CustomDrawButtonBase::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(theme_service_);
  DCHECK(chrome::NOTIFICATION_BROWSER_THEME_CHANGED == type);

  surfaces_[GTK_STATE_NORMAL]->UsePixbuf(normal_id_ ?
      theme_service_->GetRTLEnabledPixbufNamed(normal_id_) : NULL);
  surfaces_[GTK_STATE_ACTIVE]->UsePixbuf(pressed_id_ ?
      theme_service_->GetRTLEnabledPixbufNamed(pressed_id_) : NULL);
  surfaces_[GTK_STATE_PRELIGHT]->UsePixbuf(hover_id_ ?
      theme_service_->GetRTLEnabledPixbufNamed(hover_id_) : NULL);
  surfaces_[GTK_STATE_SELECTED]->UsePixbuf(NULL);
  surfaces_[GTK_STATE_INSENSITIVE]->UsePixbuf(disabled_id_ ?
      theme_service_->GetRTLEnabledPixbufNamed(disabled_id_) : NULL);
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
                   G_CALLBACK(OnEnterThunk), this);
  g_signal_connect(widget_, "leave-notify-event",
                   G_CALLBACK(OnLeaveThunk), this);
}

void CustomDrawHoverController::AnimationProgressed(
    const ui::Animation* animation) {
  gtk_widget_queue_draw(widget_);
}

gboolean CustomDrawHoverController::OnEnter(
    GtkWidget* widget,
    GdkEventCrossing* event) {
  slide_animation_.Show();
  return FALSE;
}

gboolean CustomDrawHoverController::OnLeave(
    GtkWidget* widget,
    GdkEventCrossing* event) {
  // When the user is holding a mouse button, we don't want to animate.
  if (event->state & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK))
    slide_animation_.Reset();
  else
    slide_animation_.Hide();
  return FALSE;
}

// CustomDrawButton ------------------------------------------------------------

CustomDrawButton::CustomDrawButton(int normal_id,
                                   int pressed_id,
                                   int hover_id,
                                   int disabled_id)
    : button_base_(NULL, normal_id, pressed_id, hover_id, disabled_id),
      theme_service_(NULL),
      forcing_chrome_theme_(false) {
  Init();

  // Initialize the theme stuff with no theme_provider.
  SetBrowserTheme();
}

CustomDrawButton::CustomDrawButton(GtkThemeService* theme_provider,
                                   int normal_id,
                                   int pressed_id,
                                   int hover_id,
                                   int disabled_id,
                                   const char* stock_id,
                                   GtkIconSize stock_size)
    : button_base_(theme_provider, normal_id, pressed_id, hover_id,
                   disabled_id),
      theme_service_(theme_provider),
      forcing_chrome_theme_(false) {
  native_widget_.Own(gtk_image_new_from_stock(stock_id, stock_size));

  Init();

  theme_service_->InitThemesFor(this);
  registrar_.Add(this,
                 chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(theme_provider));
}

CustomDrawButton::CustomDrawButton(GtkThemeService* theme_provider,
                                   int normal_id,
                                   int pressed_id,
                                   int hover_id,
                                   int disabled_id,
                                   GtkWidget* native_widget)
    : button_base_(theme_provider, normal_id, pressed_id, hover_id,
                   disabled_id),
      native_widget_(native_widget),
      theme_service_(theme_provider),
      forcing_chrome_theme_(false) {
  Init();

  theme_service_->InitThemesFor(this);
  registrar_.Add(this,
                 chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(theme_provider));
}

CustomDrawButton::~CustomDrawButton() {
  widget_.Destroy();
  native_widget_.Destroy();
}

void CustomDrawButton::Init() {
  widget_.Own(gtk_chrome_button_new());
  gtk_widget_set_can_focus(widget(), FALSE);
  g_signal_connect(widget(), "expose-event",
                   G_CALLBACK(OnCustomExposeThunk), this);
  hover_controller_.Init(widget());
}

void CustomDrawButton::ForceChromeTheme() {
  forcing_chrome_theme_ = true;
  SetBrowserTheme();
}

void CustomDrawButton::Observe(int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK(chrome::NOTIFICATION_BROWSER_THEME_CHANGED == type);
  SetBrowserTheme();
}

int CustomDrawButton::WidgetWidth() const {
  GtkAllocation allocation;
  gtk_widget_get_allocation(widget_.get(), &allocation);
  return allocation.width;
}

int CustomDrawButton::SurfaceWidth() const {
  return button_base_.Width();
}

int CustomDrawButton::SurfaceHeight() const {
  return button_base_.Height();
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

gboolean CustomDrawButton::OnCustomExpose(GtkWidget* sender,
                                          GdkEventExpose* e) {
  if (UseGtkTheme()) {
    // Continue processing this expose event.
    return FALSE;
  } else {
    double hover_state = hover_controller_.GetCurrentValue();
    return button_base_.OnExpose(sender, e, hover_state);
  }
}

// static
CustomDrawButton* CustomDrawButton::CloseButton(
    GtkThemeService* theme_provider) {
  CustomDrawButton* button = new CustomDrawButton(theme_provider, IDR_CLOSE_BAR,
      IDR_CLOSE_BAR_P, IDR_CLOSE_BAR_H, 0, GTK_STOCK_CLOSE, GTK_ICON_SIZE_MENU);
  return button;
}

void CustomDrawButton::SetBrowserTheme() {
  if (UseGtkTheme()) {
    if (native_widget_.get())
      gtk_button_set_image(GTK_BUTTON(widget()), native_widget_.get());
    gtk_widget_set_size_request(widget(), -1, -1);
    gtk_widget_set_app_paintable(widget(), FALSE);
  } else {
    if (native_widget_.get())
      gtk_button_set_image(GTK_BUTTON(widget()), NULL);
    gtk_widget_set_size_request(widget(), button_base_.Width(),
                                button_base_.Height());

    gtk_widget_set_app_paintable(widget(), TRUE);
  }

  gtk_chrome_button_set_use_gtk_rendering(
      GTK_CHROME_BUTTON(widget()), UseGtkTheme());
}

bool CustomDrawButton::UseGtkTheme() {
  return !forcing_chrome_theme_ && theme_service_ &&
      theme_service_->UsingNativeTheme();
}
