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

// Preferred height of the ExtensionShelfGtk.
static const int kShelfHeight = 29;

static const int kToolstripPadding = 2;

class ExtensionShelfGtk::Toolstrip {
 public:
  explicit Toolstrip(ExtensionHost* host)
      : host_(host),
        extension_name_(host_->extension()->name()) {
    Init();
  }

  ~Toolstrip() {
    label_.Destroy();
  }

  void AddToolstripToBox(GtkWidget* box);
  void RemoveToolstripFromBox(GtkWidget* box);

 private:
  void Init();

  ExtensionHost* host_;

  const std::string extension_name_;

  // Placeholder label with extension's name.
  // TODO(phajdan.jr): replace the label with rendered extension contents.
  OwnedWidgetGtk label_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Toolstrip);
};

void ExtensionShelfGtk::Toolstrip::AddToolstripToBox(GtkWidget* box) {
  gtk_box_pack_start(GTK_BOX(box), label_.get(), FALSE, FALSE,
                     kToolstripPadding);
}

void ExtensionShelfGtk::Toolstrip::RemoveToolstripFromBox(GtkWidget* box) {
  gtk_container_remove(GTK_CONTAINER(box), label_.get());
}

void ExtensionShelfGtk::Toolstrip::Init() {
  label_.Own(gtk_label_new(extension_name_.c_str()));
  gtk_widget_show_all(label_.get());
}

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

void ExtensionShelfGtk::ToolstripInsertedAt(ExtensionHost* host,
                                            int index) {
  Toolstrip* toolstrip = new Toolstrip(host);
  toolstrip->AddToolstripToBox(shelf_hbox_);
  toolstrips_.insert(toolstrip);
  model_->SetToolstripDataAt(index, toolstrip);

  AdjustHeight();
}

void ExtensionShelfGtk::ToolstripRemovingAt(ExtensionHost* host,
                                            int index) {
  Toolstrip* toolstrip = ToolstripAtIndex(index);
  toolstrip->RemoveToolstripFromBox(shelf_hbox_);
  toolstrips_.erase(toolstrip);
  model_->SetToolstripDataAt(index, NULL);
  delete toolstrip;

  AdjustHeight();
}

void ExtensionShelfGtk::ToolstripMoved(ExtensionHost* host,
                                       int from_index,
                                       int to_index) {
  // TODO(phajdan.jr): Implement reordering toolstrips.
  AdjustHeight();
}

void ExtensionShelfGtk::ToolstripChangedAt(ExtensionHost* host,
                                           int index) {
  // TODO(phajdan.jr): Implement changing toolstrips.
  AdjustHeight();
}

void ExtensionShelfGtk::ExtensionShelfEmpty() {
  AdjustHeight();
}

void ExtensionShelfGtk::ShelfModelReloaded() {
  for (std::set<Toolstrip*>::iterator iter = toolstrips_.begin();
       iter != toolstrips_.end(); ++iter) {
    (*iter)->RemoveToolstripFromBox(shelf_hbox_);
    delete *iter;
  }
  toolstrips_.clear();
  LoadFromModel();
}

void ExtensionShelfGtk::Init(Profile* profile) {
  event_box_.Own(gtk_event_box_new());

  shelf_hbox_ = gtk_hbox_new(FALSE, 0);
  gtk_widget_set_app_paintable(shelf_hbox_, TRUE);
  g_signal_connect(G_OBJECT(shelf_hbox_), "expose-event",
                   G_CALLBACK(&OnHBoxExpose), this);
  gtk_container_add(GTK_CONTAINER(event_box_.get()), shelf_hbox_);

  LoadFromModel();
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
  if (model_->empty() || toolstrips_.empty()) {
    // It's possible that |model_| is not empty, but |toolstrips_| are empty
    // when removing the last toolstrip.
    DCHECK(toolstrips_.empty());
    Hide();
  } else {
    gtk_widget_set_size_request(event_box_.get(), -1, kShelfHeight);
    Show();
  }
}

void ExtensionShelfGtk::LoadFromModel() {
  DCHECK(toolstrips_.empty());
  int count = model_->count();
  for (int i = 0; i < count; ++i)
    ToolstripInsertedAt(model_->ToolstripAt(i), i);
  AdjustHeight();
}

ExtensionShelfGtk::Toolstrip* ExtensionShelfGtk::ToolstripAtIndex(int index) {
  return static_cast<Toolstrip*>(model_->ToolstripDataAt(index));
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
