// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/extension_shelf_gtk.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/nine_box.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

ExtensionShelfGtk::ExtensionShelfGtk(Profile* profile, Browser* browser)
    : browser_(browser),
      theme_provider_(GtkThemeProvider::GetFrom(profile)),
      model_(new ExtensionShelfModel(browser)) {
  Init(profile);

  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
}

ExtensionShelfGtk::~ExtensionShelfGtk() {
  model_->RemoveObserver(this);
  event_box_.Destroy();
}

void ExtensionShelfGtk::AddShelfToBox(GtkWidget* box) {
  gtk_box_pack_end(GTK_BOX(box), event_box_.get(), FALSE, FALSE, 0);
}

void ExtensionShelfGtk::Show() {
  gtk_widget_show_all(event_box_.get());
}

void ExtensionShelfGtk::Hide() {
  gtk_widget_hide(event_box_.get());
}

void ExtensionShelfGtk::ToolstripInsertedAt(ExtensionHost* toolstrip,
                                            int index) {
  AdjustHeight();
}

void ExtensionShelfGtk::ToolstripRemovingAt(ExtensionHost* toolstrip,
                                            int index) {
  AdjustHeight();
}

void ExtensionShelfGtk::ToolstripMoved(ExtensionHost* toolstrip,
                                       int from_index,
                                       int to_index) {
  AdjustHeight();
}

void ExtensionShelfGtk::ToolstripChangedAt(ExtensionHost* toolstrip,
                                           int index) {
  AdjustHeight();
}

void ExtensionShelfGtk::ExtensionShelfEmpty() {
  AdjustHeight();
}

void ExtensionShelfGtk::ShelfModelReloaded() {
  AdjustHeight();
}

void ExtensionShelfGtk::Init(Profile* profile) {
  event_box_.Own(gtk_event_box_new());

  shelf_hbox_ = gtk_hbox_new(FALSE, 0);
  gtk_widget_set_app_paintable(shelf_hbox_, TRUE);
  g_signal_connect(G_OBJECT(shelf_hbox_), "expose-event",
                   G_CALLBACK(&OnHBoxExpose), this);
  gtk_container_add(GTK_CONTAINER(event_box_.get()), shelf_hbox_);

  label_ = gtk_label_new("(extension shelf will appear here)");
  gtk_box_pack_start(GTK_BOX(shelf_hbox_), label_,
                     TRUE, TRUE, 0);

  AdjustHeight();

  model_->AddObserver(this);
}

void ExtensionShelfGtk::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  if (type == NotificationType::BROWSER_THEME_CHANGED) {
    // TODO(phajdan.jr): Handle theme changes.
  } else {
    NOTREACHED() << "unexpected notification";
  }
}

void ExtensionShelfGtk::InitBackground() {
  if (background_ninebox_.get())
    return;

  background_ninebox_.reset(new NineBox(
      browser_->profile()->GetThemeProvider(),
      0, IDR_THEME_TOOLBAR, 0, 0, 0, 0, 0, 0, 0));
}

void ExtensionShelfGtk::AdjustHeight() {
  int target_height = model_->empty() ? 0 : event_box_->requisition.height;
  gtk_widget_set_size_request(event_box_.get(), -1, target_height);
}

// static
gboolean ExtensionShelfGtk::OnHBoxExpose(GtkWidget* widget,
                                         GdkEventExpose* event,
                                         ExtensionShelfGtk* bar) {
  // Paint the background theme image.
  cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(widget->window));
  cairo_rectangle(cr, event->area.x, event->area.y,
                  event->area.width, event->area.height);
  cairo_clip(cr);
  bar->InitBackground();
  bar->background_ninebox_->RenderTopCenterStrip(
      cr, event->area.x, event->area.y,
      event->area.x + event->area.width);
  cairo_destroy(cr);

  return FALSE;  // Propagate expose to children.
}
