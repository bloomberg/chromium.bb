// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/throbber_gtk.h"

#include "base/logging.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_source.h"
#include "grit/ui_resources.h"
#include "ui/base/animation/tween.h"
#include "ui/gfx/image/cairo_cached_surface.h"

namespace {

// The length of a single cycle of the animation in milliseconds.
const int kThrobberDurationMs = 750;

}  // namespace

ThrobberGtk::ThrobberGtk(GtkThemeService* theme_service)
    : theme_service_(theme_service),
      animation_(this),
      num_frames_(0) {
  DCHECK(theme_service_);
  Init();
}

ThrobberGtk::~ThrobberGtk() {
  widget_.Destroy();
}

void ThrobberGtk::Start() {
  animation_.Show();
}

void ThrobberGtk::Stop() {
  animation_.Reset();
}

void ThrobberGtk::AnimationEnded(const ui::Animation* animation) {
  animation_.Reset();
  animation_.Show();
}

void ThrobberGtk::AnimationProgressed(const ui::Animation* animation) {
  gtk_widget_queue_draw(widget_.get());
}

void ThrobberGtk::Observe(int type,
                          const content::NotificationSource& source,
                          const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_BROWSER_THEME_CHANGED, type);
  LoadFrames();
  gtk_widget_queue_draw(widget_.get());
}

gboolean ThrobberGtk::OnExpose(GtkWidget* widget, GdkEventExpose* expose) {
  cairo_t* cairo_context =
      gdk_cairo_create(GDK_DRAWABLE(gtk_widget_get_window(widget)));
  GtkAllocation allocation;
  gtk_widget_get_allocation(widget, &allocation);
  cairo_translate(cairo_context, allocation.x, allocation.y);

  gfx::CairoCachedSurface* cairo_frames = frames_.ToCairo();
  const int frame =
      static_cast<int>(animation_.GetCurrentValue() * (num_frames_ - 1));
  const int image_size = cairo_frames->Height();
  const int image_offset = frame * image_size;

  cairo_frames->SetSource(cairo_context, widget, -image_offset, 0);
  cairo_rectangle(cairo_context, 0, 0, image_size, image_size);
  cairo_fill(cairo_context);
  cairo_destroy(cairo_context);

  return TRUE;
}

void ThrobberGtk::Init() {
  animation_.SetSlideDuration(kThrobberDurationMs);
  animation_.SetTweenType(ui::Tween::LINEAR);
  widget_.Own(gtk_image_new());
  gtk_widget_set_can_focus(widget_.get(), FALSE);
  g_signal_connect(widget_.get(), "expose-event", G_CALLBACK(OnExposeThunk),
                   this);

  theme_service_->InitThemesFor(this);
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(theme_service_));
}

void ThrobberGtk::LoadFrames() {
  frames_ = theme_service_->GetImageNamed(IDR_THROBBER);
  DCHECK(!frames_.IsEmpty());
  const int width = frames_.ToCairo()->Width();
  const int height = frames_.ToCairo()->Height();
  DCHECK_EQ(0, width % height);
  num_frames_ = width / height;

  gtk_widget_set_size_request(widget_.get(), height, height);
}
