// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/tab_contents_container_gtk.h"

#include <algorithm>

#include "base/i18n/rtl.h"
#include "chrome/browser/ui/gtk/status_bubble_gtk.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/base/gtk/gtk_expanded_container.h"
#include "ui/base/gtk/gtk_floating_container.h"
#include "ui/gfx/native_widget_types.h"

namespace {
void HideWidget(GtkWidget* widget, gpointer ignored) {
  gtk_widget_hide(widget);
}
}  // namespace

TabContentsContainerGtk::TabContentsContainerGtk(StatusBubbleGtk* status_bubble,
                                                 bool embed_fullscreen_widget)
    : status_bubble_(status_bubble),
      should_embed_fullscreen_widgets_(embed_fullscreen_widget),
      is_embedding_fullscreen_widget_(false) {
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

TabContentsContainerGtk::~TabContentsContainerGtk() {
  floating_.Destroy();
}

void TabContentsContainerGtk::SetTab(content::WebContents* tab) {
  if (tab == web_contents())
    return;

  HideTab();
  WebContentsObserver::Observe(tab);
  is_embedding_fullscreen_widget_ =
      should_embed_fullscreen_widgets_ &&
      tab && tab->GetFullscreenRenderWidgetHostView();
  PackTab();
}

void TabContentsContainerGtk::PackTab() {
  content::WebContents* const tab = web_contents();
  if (!tab)
    return;

  const gfx::NativeView widget = is_embedding_fullscreen_widget_ ?
      tab->GetFullscreenRenderWidgetHostView()->GetNativeView() :
      tab->GetView()->GetNativeView();
  if (widget) {
    if (gtk_widget_get_parent(widget) != expanded_)
      gtk_container_add(GTK_CONTAINER(expanded_), widget);
    gtk_widget_show(widget);
  }

  if (is_embedding_fullscreen_widget_)
    return;

  tab->WasShown();

  // Make sure that the tab is below the find bar. Sometimes the content
  // native view will be null.
  GtkWidget* const content_widget = tab->GetView()->GetContentNativeView();
  if (content_widget) {
    GdkWindow* const content_gdk_window = gtk_widget_get_window(content_widget);
    if (content_gdk_window)
      gdk_window_lower(content_gdk_window);
  }
}

void TabContentsContainerGtk::HideTab() {
  content::WebContents* const tab = web_contents();
  if (!tab)
    return;

  gtk_container_foreach(GTK_CONTAINER(expanded_), &HideWidget, NULL);
  if (is_embedding_fullscreen_widget_)
    return;
  tab->WasHidden();
}

void TabContentsContainerGtk::DetachTab(content::WebContents* tab) {
  if (!tab)
    return;
  if (tab == web_contents()) {
    HideTab();
    WebContentsObserver::Observe(NULL);
  }

  const gfx::NativeView widget = tab->GetView()->GetNativeView();
  const gfx::NativeView fs_widget =
      (should_embed_fullscreen_widgets_ &&
       tab->GetFullscreenRenderWidgetHostView()) ?
          tab->GetFullscreenRenderWidgetHostView()->GetNativeView() : NULL;

  // It is possible to detach an unrealized, unparented WebContents if you
  // slow things down enough in valgrind. Might happen in the real world, too.
  if (widget) {
    GtkWidget* const parent = gtk_widget_get_parent(widget);
    if (parent) {
      DCHECK_EQ(parent, expanded_);
      gtk_container_remove(GTK_CONTAINER(expanded_), widget);
    }
  }
  if (fs_widget) {
    GtkWidget* const parent = gtk_widget_get_parent(fs_widget);
    if (parent) {
      DCHECK_EQ(parent, expanded_);
      gtk_container_remove(GTK_CONTAINER(expanded_), fs_widget);
    }
  }
}

void TabContentsContainerGtk::WebContentsDestroyed(
    content::WebContents* contents) {
  // Sometimes, a WebContents is destroyed before we know about it. This allows
  // us to clean up our state in case this happens.
  DetachTab(contents);
}

// -----------------------------------------------------------------------------
// ViewIDUtil::Delegate implementation

GtkWidget* TabContentsContainerGtk::GetWidgetForViewID(ViewID view_id) {
  if (view_id == VIEW_ID_TAB_CONTAINER)
    return widget();

  return NULL;
}

// -----------------------------------------------------------------------------

void TabContentsContainerGtk::DidShowFullscreenWidget(int routing_id) {
  if (!should_embed_fullscreen_widgets_)
    return;
  HideTab();
  is_embedding_fullscreen_widget_ =
      web_contents() && web_contents()->GetFullscreenRenderWidgetHostView();
  PackTab();
}

void TabContentsContainerGtk::DidDestroyFullscreenWidget(int routing_id) {
  if (!should_embed_fullscreen_widgets_)
    return;
  HideTab();
  is_embedding_fullscreen_widget_ = false;
  PackTab();
}

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
