// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/custom_button.h"

#include "app/gfx/gtk_util.h"
#include "app/gfx/skbitmap_operations.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "app/theme_provider.h"
#include "base/basictypes.h"
#include "chrome/browser/gtk/cairo_cached_surface.h"
#include "chrome/browser/gtk/gtk_chrome_button.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "grit/theme_resources.h"

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
      theme_provider_(theme_provider) {
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

gboolean CustomDrawButtonBase::OnExpose(GtkWidget* widget, GdkEventExpose* e) {
  CairoCachedSurface* pixbuf =
      surfaces_[paint_override_ >= 0 ?
                paint_override_ : GTK_WIDGET_STATE(widget)].get();

  // Fall back to the default image if we don't have one for this state.
  if (!pixbuf || !pixbuf->valid())
    pixbuf = surfaces_[GTK_STATE_NORMAL].get();

  if (!pixbuf || !pixbuf->valid())
    return FALSE;

  cairo_t* cairo_context = gdk_cairo_create(GDK_DRAWABLE(widget->window));
  cairo_translate(cairo_context, widget->allocation.x, widget->allocation.y);

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
  GTK_WIDGET_UNSET_FLAGS(widget_.get(), GTK_CAN_FOCUS);
  g_signal_connect(G_OBJECT(widget_.get()), "expose-event",
                   G_CALLBACK(OnCustomExpose), this);
}

void CustomDrawButton::Observe(NotificationType type,
    const NotificationSource& source, const NotificationDetails& details) {
  DCHECK(NotificationType::BROWSER_THEME_CHANGED == type);
  SetBrowserTheme();
}

void CustomDrawButton::SetPaintOverride(GtkStateType state) {
  button_base_.set_paint_override(state);
  gtk_chrome_button_set_paint_state(GTK_CHROME_BUTTON(widget_.get()), state);
  gtk_widget_queue_draw(widget_.get());
}

void CustomDrawButton::UnsetPaintOverride() {
  button_base_.set_paint_override(-1);
  gtk_chrome_button_unset_paint_state(GTK_CHROME_BUTTON(widget_.get()));
  gtk_widget_queue_draw(widget_.get());
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
    return button->button_base_.OnExpose(widget, e);
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
        GTK_BUTTON(widget_.get()),
        gtk_image_new_from_stock(gtk_stock_name_, icon_size_));
    gtk_widget_set_size_request(widget_.get(), -1, -1);
    gtk_widget_set_app_paintable(widget_.get(), FALSE);
    gtk_widget_set_double_buffered(widget_.get(), TRUE);
  } else {
    gtk_widget_set_size_request(widget_.get(), button_base_.Width(),
                                button_base_.Height());

    gtk_widget_set_app_paintable(widget_.get(), TRUE);
    // We effectively double-buffer by virtue of having only one image...
    gtk_widget_set_double_buffered(widget_.get(), FALSE);
  }

  gtk_chrome_button_set_use_gtk_rendering(
      GTK_CHROME_BUTTON(widget_.get()), use_gtk);
}
