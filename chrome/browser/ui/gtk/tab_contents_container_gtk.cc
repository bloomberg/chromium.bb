// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/tab_contents_container_gtk.h"

#include <algorithm>

#include "base/i18n/rtl.h"
#include "chrome/browser/ui/gtk/status_bubble_gtk.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/base/gtk/gtk_expanded_container.h"
#include "ui/base/gtk/gtk_floating_container.h"
#include "ui/gfx/native_widget_types.h"

TabContentsContainerGtk::TabContentsContainerGtk(StatusBubbleGtk* status_bubble)
    : tab_(NULL),
      preview_(NULL),
      status_bubble_(status_bubble) {
  Init();
}

TabContentsContainerGtk::~TabContentsContainerGtk() {
  floating_.Destroy();
}

void TabContentsContainerGtk::Init() {
  // A high level overview of the TabContentsContainer:
  //
  // +- GtkFloatingContainer |floating_| -------------------------------+
  // |+- GtkExpandedContainer |expanded_| -----------------------------+|
  // ||                                                                ||
  // ||                                                                ||
  // ||                                                                ||
  // ||                                                                ||
  // |+- (StatusBubble) ------+                                        ||
  // |+                       +                                        ||
  // |+-----------------------+----------------------------------------+|
  // +------------------------------------------------------------------+

  floating_.Own(gtk_floating_container_new());
  gtk_widget_set_name(floating_.get(), "chrome-tab-contents-container");
  g_signal_connect(floating_.get(), "focus", G_CALLBACK(OnFocusThunk), this);

  expanded_ = gtk_expanded_container_new();
  gtk_container_add(GTK_CONTAINER(floating_.get()), expanded_);

  if (status_bubble_) {
    gtk_floating_container_add_floating(GTK_FLOATING_CONTAINER(floating_.get()),
                                        status_bubble_->widget());
    g_signal_connect(floating_.get(), "set-floating-position",
                     G_CALLBACK(OnSetFloatingPosition), this);
  }

  gtk_widget_show(expanded_);
  gtk_widget_show(floating_.get());

  ViewIDUtil::SetDelegateForWidget(widget(), this);
}

void TabContentsContainerGtk::SetTab(content::WebContents* tab) {
  if (tab_ == tab)
    return;

  if (tab_)
    HideTab(tab_);

  tab_ = tab;

  if (tab_) {
    // If the preview is becoming the new permanent tab, we just reassign some
    // pointers. Otherwise, we have to actually add it to the widget hierarchy.
    if (tab_ == preview_)
      preview_ = NULL;
    else
      PackTab(tab_);

    // Make sure that the tab is below the find bar. Sometimes the content
    // native view will be null.
    GtkWidget* widget = tab_->GetView()->GetContentNativeView();
    if (widget) {
      GdkWindow* content_gdk_window = gtk_widget_get_window(widget);
      if (content_gdk_window)
        gdk_window_lower(content_gdk_window);
    }
  }
}

void TabContentsContainerGtk::SetPreview(content::WebContents* preview) {
  if (preview_ == preview)
    return;

  if (preview_) {
    HideTab(preview_);
    GtkWidget* preview_widget = preview_->GetView()->GetNativeView();
    if (preview_widget)
      gtk_container_remove(GTK_CONTAINER(expanded_), preview_widget);
  }

  preview_ = preview;

  if (preview_)
    PackTab(preview_);
}

void TabContentsContainerGtk::PackTab(content::WebContents* tab) {
  gfx::NativeView widget = tab->GetView()->GetNativeView();
  if (widget) {
    if (gtk_widget_get_parent(widget) != expanded_)
      gtk_container_add(GTK_CONTAINER(expanded_), widget);
    gtk_widget_show(widget);
  }

  tab->WasShown();
  registrar_.Add(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                 content::Source<content::WebContents>(tab));
}

void TabContentsContainerGtk::HideTab(content::WebContents* tab) {
  gfx::NativeView widget = tab->GetView()->GetNativeView();
  if (widget)
    gtk_widget_hide(widget);

  tab->WasHidden();
  registrar_.Remove(this, content::NOTIFICATION_WEB_CONTENTS_DESTROYED,
                    content::Source<content::WebContents>(tab));
}

void TabContentsContainerGtk::DetachTab(content::WebContents* tab) {
  gfx::NativeView widget = tab->GetView()->GetNativeView();

  // It is possible to detach an unrealized, unparented WebContents if you
  // slow things down enough in valgrind. Might happen in the real world, too.
  if (widget) {
    GtkWidget* parent = gtk_widget_get_parent(widget);
    if (parent) {
      DCHECK_EQ(parent, expanded_);
      gtk_container_remove(GTK_CONTAINER(expanded_), widget);
    }
  }
}

void TabContentsContainerGtk::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(content::NOTIFICATION_WEB_CONTENTS_DESTROYED, type);
  WebContentsDestroyed(content::Source<content::WebContents>(source).ptr());
}

void TabContentsContainerGtk::WebContentsDestroyed(
    content::WebContents* contents) {
  // Sometimes, a WebContents is destroyed before we know about it. This allows
  // us to clean up our state in case this happens.
  if (contents == preview_)
    SetPreview(NULL);
  else if (contents == tab_)
    SetTab(NULL);
  else
    NOTREACHED();
}

// Prevent |preview_| from getting focus via the tab key. If |tab_| exists, try
// to focus that. Otherwise, do nothing, but stop event propagation. See bug
// http://crbug.com/63365
gboolean TabContentsContainerGtk::OnFocus(GtkWidget* widget,
                                          GtkDirectionType focus) {
  if (preview_) {
    gtk_widget_child_focus(tab_->GetView()->GetContentNativeView(), focus);
    return TRUE;
  }

  // No preview contents; let the default handler run.
  return FALSE;
}

// -----------------------------------------------------------------------------
// ViewIDUtil::Delegate implementation

GtkWidget* TabContentsContainerGtk::GetWidgetForViewID(ViewID view_id) {
  if (view_id == VIEW_ID_TAB_CONTAINER)
    return widget();

  return NULL;
}

// -----------------------------------------------------------------------------

// static
void TabContentsContainerGtk::OnSetFloatingPosition(
    GtkFloatingContainer* floating_container, GtkAllocation* allocation,
    TabContentsContainerGtk* tab_contents_container) {
  StatusBubbleGtk* status = tab_contents_container->status_bubble_;

  // Look at the size request of the status bubble and tell the
  // GtkFloatingContainer where we want it positioned.
  GtkRequisition requisition;
  gtk_widget_size_request(status->widget(), &requisition);

  bool ltr = !base::i18n::IsRTL();

  GValue value = { 0, };
  g_value_init(&value, G_TYPE_INT);
  if (ltr ^ status->flip_horizontally())  // Is it on the left?
    g_value_set_int(&value, 0);
  else
    g_value_set_int(&value, allocation->width - requisition.width);
  gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                   status->widget(), "x", &value);

  int child_y = std::max(allocation->height - requisition.height, 0);
  g_value_set_int(&value, child_y + status->y_offset());
  gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                   status->widget(), "y", &value);
  g_value_unset(&value);
}
