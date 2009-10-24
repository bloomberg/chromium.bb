// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/toolbar_star_toggle_gtk.h"

#include "app/gtk_dnd_util.h"
#include "app/resource_bundle.h"
#include "base/gfx/rect.h"
#include "chrome/browser/browser.h"
#include "chrome/browser/gtk/bookmark_bubble_gtk.h"
#include "chrome/browser/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/gtk/gtk_chrome_button.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/profile.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/common/gtk_util.h"
#include "chrome/common/notification_service.h"
#include "grit/theme_resources.h"

ToolbarStarToggleGtk::ToolbarStarToggleGtk(BrowserToolbarGtk* host)
    : host_(host),
      widget_(gtk_chrome_button_new()),
      is_starred_(false),
      theme_provider_(GtkThemeProvider::GetFrom(host->profile())),
      unstarred_(theme_provider_, IDR_STAR, IDR_STAR_P, IDR_STAR_H, IDR_STAR_D),
      starred_(theme_provider_, IDR_STARRED, IDR_STARRED_P, IDR_STARRED_H, 0) {
  gtk_widget_set_size_request(widget_.get(), unstarred_.Width(),
                              unstarred_.Height());

  gtk_widget_set_app_paintable(widget_.get(), TRUE);
  // We effectively double-buffer by virtue of having only one image...
  gtk_widget_set_double_buffered(widget_.get(), FALSE);

  g_signal_connect(widget(), "expose-event",
                   G_CALLBACK(OnExpose), this);
  GTK_WIDGET_UNSET_FLAGS(widget_.get(), GTK_CAN_FOCUS);

  gtk_drag_source_set(widget(), GDK_BUTTON1_MASK,
                      NULL, 0, GDK_ACTION_COPY);
  GtkDndUtil::SetSourceTargetListFromCodeMask(widget(),
                                              GtkDndUtil::TEXT_PLAIN |
                                              GtkDndUtil::TEXT_URI_LIST |
                                              GtkDndUtil::CHROME_NAMED_URL);
  g_signal_connect(widget(), "drag-data-get", G_CALLBACK(OnDragDataGet), this);

  theme_provider_->InitThemesFor(this);
  registrar_.Add(this,
                 NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
}

ToolbarStarToggleGtk::~ToolbarStarToggleGtk() {
  widget_.Destroy();
}

void ToolbarStarToggleGtk::Observe(NotificationType type,
    const NotificationSource& source, const NotificationDetails& details) {
  DCHECK(NotificationType::BROWSER_THEME_CHANGED == type);

  GtkThemeProvider* provider = static_cast<GtkThemeProvider*>(
      Source<GtkThemeProvider>(source).ptr());
  DCHECK(provider == theme_provider_);
  UpdateGTKButton();
}

void ToolbarStarToggleGtk::ShowStarBubble(const GURL& url,
                                          bool newly_bookmarked) {
  GtkWidget* widget = widget_.get();
  BookmarkBubbleGtk::Show(GTK_WINDOW(gtk_widget_get_toplevel(widget)),
                          gtk_util::GetWidgetRectRelativeToToplevel(widget),
                          host_->profile(),
                          url,
                          newly_bookmarked);
}

void ToolbarStarToggleGtk::SetStarred(bool starred) {
  is_starred_ = starred;
  gtk_widget_queue_draw(widget_.get());
  UpdateGTKButton();
}

// static
gboolean ToolbarStarToggleGtk::OnExpose(GtkWidget* widget, GdkEventExpose* e,
                                        ToolbarStarToggleGtk* button) {
  if (button->theme_provider_->UseGtkTheme()) {
    return FALSE;
  } else {
    if (button->is_starred_) {
      return button->starred_.OnExpose(widget, e);
    } else {
      return button->unstarred_.OnExpose(widget, e);
    }
  }
}

// static
void ToolbarStarToggleGtk::OnDragDataGet(GtkWidget* widget,
    GdkDragContext* drag_context, GtkSelectionData* data, guint info,
    guint time, ToolbarStarToggleGtk* star) {
  const TabContents* tab = star->host_->browser()->tabstrip_model()->
      GetSelectedTabContents();
  if (!tab)
    return;
  GtkDndUtil::WriteURLWithName(data, tab->GetURL(), tab->GetTitle(), info);
}

void ToolbarStarToggleGtk::UpdateGTKButton() {
  bool use_gtk = theme_provider_ && theme_provider_->UseGtkTheme();

  if (use_gtk) {
    GdkPixbuf* pixbuf = NULL;
    if (is_starred_) {
      pixbuf = theme_provider_->GetPixbufNamed(IDR_STARRED_NOBORDER_CENTER);
    } else {
      pixbuf = theme_provider_->GetPixbufNamed(IDR_STAR_NOBORDER_CENTER);
    }

    gtk_button_set_image(
        GTK_BUTTON(widget_.get()),
        gtk_image_new_from_pixbuf(pixbuf));

    gtk_widget_set_size_request(widget_.get(), -1, -1);
    gtk_widget_set_app_paintable(widget_.get(), FALSE);
    gtk_widget_set_double_buffered(widget_.get(), TRUE);
  } else {
    gtk_widget_set_size_request(widget_.get(), unstarred_.Width(),
                                unstarred_.Height());

    gtk_widget_set_app_paintable(widget_.get(), TRUE);
    // We effectively double-buffer by virtue of having only one image...
    gtk_widget_set_double_buffered(widget_.get(), FALSE);
  }

  gtk_chrome_button_set_use_gtk_rendering(
      GTK_CHROME_BUTTON(widget_.get()), use_gtk);
}
