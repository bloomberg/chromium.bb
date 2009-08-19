// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/extension_shelf_gtk.h"

#include "chrome/browser/browser.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/nine_box.h"
#include "chrome/browser/gtk/tabs/tab_strip_gtk.h"
#include "chrome/browser/profile.h"
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
    DCHECK(host_->view());
    Init();
  }

  void AddToolstripToBox(GtkWidget* box);
  void RemoveToolstripFromBox(GtkWidget* box);

 private:
  void Init();

  gfx::NativeView native_view() {
    return host_->view()->native_view();
  }

  ExtensionHost* host_;

  const std::string extension_name_;

 private:
  DISALLOW_COPY_AND_ASSIGN(Toolstrip);
};

void ExtensionShelfGtk::Toolstrip::AddToolstripToBox(GtkWidget* box) {
  gtk_box_pack_start(GTK_BOX(box), native_view(), FALSE, FALSE,
                     kToolstripPadding);
}

void ExtensionShelfGtk::Toolstrip::RemoveToolstripFromBox(GtkWidget* box) {
  gtk_container_remove(GTK_CONTAINER(box), native_view());
}

void ExtensionShelfGtk::Toolstrip::Init() {
  host_->view()->set_is_toolstrip(true);
  gtk_widget_show_all(native_view());
}

ExtensionShelfGtk::ExtensionShelfGtk(Profile* profile, Browser* browser)
    : browser_(browser),
      theme_provider_(GtkThemeProvider::GetFrom(profile)),
      model_(browser->extension_shelf_model()) {
  Init(profile);
}

ExtensionShelfGtk::~ExtensionShelfGtk() {
  if (model_)
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

void ExtensionShelfGtk::ToolstripChanged(
    ExtensionShelfModel::iterator toolstrip) {
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

void ExtensionShelfGtk::ShelfModelDeleting() {
  for (std::set<Toolstrip*>::iterator iter = toolstrips_.begin();
       iter != toolstrips_.end(); ++iter) {
    (*iter)->RemoveToolstripFromBox(shelf_hbox_);
    delete *iter;
  }
  toolstrips_.clear();

  model_->RemoveObserver(this);
  model_ = NULL;
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
    ToolstripInsertedAt(model_->ToolstripAt(i).host, i);
  AdjustHeight();
}

ExtensionShelfGtk::Toolstrip* ExtensionShelfGtk::ToolstripAtIndex(int index) {
  return static_cast<Toolstrip*>(model_->ToolstripAt(index).data);
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
  gfx::Point tabstrip_origin =
      static_cast<BrowserWindowGtk*>(bar->browser_->window())->
          tabstrip()->GetTabStripOriginForWidget(widget);
  GdkPixbuf* background = bar->browser_->profile()->GetThemeProvider()->
      GetPixbufNamed(IDR_THEME_TOOLBAR);
  gdk_cairo_set_source_pixbuf(cr, background,
                              tabstrip_origin.x(), tabstrip_origin.y());
  cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
  cairo_rectangle(cr, tabstrip_origin.x(), tabstrip_origin.y(),
                      event->area.x + event->area.width - tabstrip_origin.x(),
                      gdk_pixbuf_get_height(background));
  cairo_fill(cr);
  cairo_destroy(cr);

  return FALSE;  // Propagate expose to children.
}
