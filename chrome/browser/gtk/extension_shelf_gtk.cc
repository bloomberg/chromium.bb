// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/extension_shelf_gtk.h"

#include "base/gfx/gtk_util.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/gtk/browser_window_gtk.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/profile.h"
#include "chrome/common/notification_service.h"
#include "grit/app_resources.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"

// Background color of the shelf.
static const GdkColor kBackgroundColor = GDK_COLOR_RGB(230, 237, 244);

// Border color (the top pixel of the shelf).
const GdkColor kBorderColor = GDK_COLOR_RGB(214, 214, 214);

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

  void SetBackground(const SkBitmap& background) {
    host_->view()->SetBackground(background);
  }

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
  InitBackground();
  Toolstrip* toolstrip = new Toolstrip(host);
  toolstrip->SetBackground(*background_.get());
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

void ExtensionShelfGtk::Observe(NotificationType type,
                                const NotificationSource& source,
                                const NotificationDetails& details) {
  DCHECK(type == NotificationType::BROWSER_THEME_CHANGED);
  if (theme_provider_->UseGtkTheme()) {
    GdkColor color = theme_provider_->GetBorderColor();
    gtk_widget_modify_bg(top_border_, GTK_STATE_NORMAL, &color);
  } else {
    gtk_widget_modify_bg(top_border_, GTK_STATE_NORMAL, &kBorderColor);
  }
}

void ExtensionShelfGtk::Init(Profile* profile) {
  top_border_ = gtk_event_box_new();
  gtk_widget_set_size_request(GTK_WIDGET(top_border_), 0, 1);

  // The event box provides a background for the shelf and is its top-level
  // widget.
  event_box_.Own(gtk_event_box_new());
  gtk_widget_modify_bg(event_box_.get(), GTK_STATE_NORMAL, &kBackgroundColor);

  shelf_hbox_ = gtk_hbox_new(FALSE, 0);

  GtkWidget* vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), top_border_, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), shelf_hbox_, FALSE, FALSE, 0);

  gtk_container_add(GTK_CONTAINER(event_box_.get()), vbox);

  theme_provider_->InitThemesFor(this);
  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());

  LoadFromModel();
  model_->AddObserver(this);
}

void ExtensionShelfGtk::InitBackground() {
  if (background_.get())
    return;
  background_.reset(new SkBitmap);
  background_->setConfig(SkBitmap::kARGB_8888_Config, 3, 3);
  background_->allocPixels();
  background_->eraseRGB(kBackgroundColor.red >> 8,
                        kBackgroundColor.green >> 8,
                        kBackgroundColor.blue >> 8);
}

void ExtensionShelfGtk::AdjustHeight() {
  if (model_->empty() || toolstrips_.empty()) {
    // It's possible that |model_| is not empty, but |toolstrips_| are empty
    // when removing the last toolstrip.
    DCHECK(toolstrips_.empty());
    Hide();
  } else {
    gtk_widget_set_size_request(shelf_hbox_, -1, kShelfHeight);
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
