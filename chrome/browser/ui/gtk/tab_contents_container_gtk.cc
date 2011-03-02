// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/tab_contents_container_gtk.h"

#include <algorithm>

#include "base/i18n/rtl.h"
#include "chrome/browser/renderer_host/render_widget_host_view_gtk.h"
#include "chrome/browser/ui/gtk/gtk_expanded_container.h"
#include "chrome/browser/ui/gtk/gtk_floating_container.h"
#include "chrome/browser/ui/gtk/status_bubble_gtk.h"
#include "chrome/common/notification_source.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "ui/gfx/native_widget_types.h"

TabContentsContainerGtk::TabContentsContainerGtk(StatusBubbleGtk* status_bubble)
    : tab_contents_(NULL),
      preview_contents_(NULL),
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

void TabContentsContainerGtk::SetTabContents(TabContents* tab_contents) {
  HideTabContents(tab_contents_);
  if (tab_contents_) {
    registrar_.Remove(this, NotificationType::TAB_CONTENTS_DESTROYED,
                      Source<TabContents>(tab_contents_));
  }

  tab_contents_ = tab_contents;

  if (tab_contents_ == preview_contents_) {
    // If the preview contents is becoming the new permanent tab contents, we
    // just reassign some pointers.
    preview_contents_ = NULL;
  } else if (tab_contents_) {
    // Otherwise we actually have to add it to the widget hierarchy.
    PackTabContents(tab_contents);
    registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                   Source<TabContents>(tab_contents_));
  }
}

TabContents* TabContentsContainerGtk::GetVisibleTabContents() {
  return preview_contents_ ? preview_contents_ : tab_contents_;
}

void TabContentsContainerGtk::SetPreviewContents(TabContents* preview) {
  if (preview_contents_)
    RemovePreviewContents();
  else
    HideTabContents(tab_contents_);

  preview_contents_ = preview;

  PackTabContents(preview);
  registrar_.Add(this, NotificationType::TAB_CONTENTS_DESTROYED,
                 Source<TabContents>(preview_contents_));
}

void TabContentsContainerGtk::RemovePreviewContents() {
  if (!preview_contents_)
    return;

  HideTabContents(preview_contents_);

  GtkWidget* preview_widget = preview_contents_->GetNativeView();
  if (preview_widget)
    gtk_container_remove(GTK_CONTAINER(expanded_), preview_widget);

  registrar_.Remove(this, NotificationType::TAB_CONTENTS_DESTROYED,
                    Source<TabContents>(preview_contents_));
  preview_contents_ = NULL;
}

void TabContentsContainerGtk::PopPreviewContents() {
  if (!preview_contents_)
    return;

  RemovePreviewContents();

  PackTabContents(tab_contents_);
}

void TabContentsContainerGtk::PackTabContents(TabContents* contents) {
  if (!contents)
    return;

  gfx::NativeView widget = contents->GetNativeView();
  if (widget) {
    if (widget->parent != expanded_)
      gtk_container_add(GTK_CONTAINER(expanded_), widget);
    gtk_widget_show(widget);
  }

  // We need to make sure that we are below the findbar.
  // Sometimes the content native view will be null.
  if (contents->GetContentNativeView()) {
    GdkWindow* content_gdk_window =
        contents->GetContentNativeView()->window;
    if (content_gdk_window)
      gdk_window_lower(content_gdk_window);
  }

  contents->ShowContents();
}

void TabContentsContainerGtk::HideTabContents(TabContents* contents) {
  if (!contents)
    return;

  gfx::NativeView widget = contents->GetNativeView();
  if (widget)
    gtk_widget_hide(widget);

  contents->WasHidden();
}

void TabContentsContainerGtk::DetachTabContents(TabContents* tab_contents) {
  gfx::NativeView widget = tab_contents->GetNativeView();

  // It is possible to detach an unrealized, unparented TabContents if you
  // slow things down enough in valgrind. Might happen in the real world, too.
  if (widget && widget->parent) {
    DCHECK_EQ(widget->parent, expanded_);
    gtk_container_remove(GTK_CONTAINER(expanded_), widget);
  }
}

void TabContentsContainerGtk::Observe(NotificationType type,
                                      const NotificationSource& source,
                                      const NotificationDetails& details) {
  DCHECK(type == NotificationType::TAB_CONTENTS_DESTROYED);

  TabContentsDestroyed(Source<TabContents>(source).ptr());
}

void TabContentsContainerGtk::TabContentsDestroyed(TabContents* contents) {
  // Sometimes, a TabContents is destroyed before we know about it. This allows
  // us to clean up our state in case this happens.
  if (contents == preview_contents_)
    PopPreviewContents();
  else if (contents == tab_contents_)
    SetTabContents(NULL);
  else
    NOTREACHED();
}

// Prevent |preview_contents_| from getting focus via the tab key. If
// |tab_contents_| exists, try to focus that. Otherwise, do nothing, but stop
// event propagation. See bug http://crbug.com/63365
gboolean TabContentsContainerGtk::OnFocus(GtkWidget* widget,
                                          GtkDirectionType focus) {
  if (preview_contents_) {
    gtk_widget_child_focus(tab_contents_->GetContentNativeView(), focus);
    return TRUE;
  }

  // No preview contents; let the default handler run.
  return FALSE;
}

// -----------------------------------------------------------------------------
// ViewIDUtil::Delegate implementation

GtkWidget* TabContentsContainerGtk::GetWidgetForViewID(ViewID view_id) {
  if (view_id == VIEW_ID_TAB_CONTAINER ||
      view_id == VIEW_ID_TAB_CONTAINER_FOCUS_VIEW) {
    return widget();
  }

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
