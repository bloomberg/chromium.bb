// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/constrained_window_gtk.h"

#include <gdk/gdkkeysyms.h>

#include "chrome/browser/browser_list.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "chrome/browser/tab_contents/tab_contents_view_gtk.h"

ConstrainedWindowGtk::ConstrainedWindowGtk(
    TabContents* owner, ConstrainedWindowGtkDelegate* delegate)
    : owner_(owner),
      delegate_(delegate),
      visible_(false),
      accel_group_(gtk_accel_group_new()) {
  DCHECK(owner);
  DCHECK(delegate);
  GtkWidget* dialog = delegate->GetWidgetRoot();

  // Unlike other users of CreateBorderBin, we need a dedicated frame around
  // our "window".
  GtkWidget* ebox = gtk_event_box_new();
  GtkWidget* frame = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);
  GtkWidget* alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment),
      gtk_util::kContentAreaBorder, gtk_util::kContentAreaBorder,
      gtk_util::kContentAreaBorder, gtk_util::kContentAreaBorder);
  gtk_container_add(GTK_CONTAINER(alignment), dialog);
  gtk_container_add(GTK_CONTAINER(frame), alignment);
  gtk_container_add(GTK_CONTAINER(ebox), frame);
  border_.Own(ebox);
  ConnectAccelerators();
}

ConstrainedWindowGtk::~ConstrainedWindowGtk() {
  border_.Destroy();

  gtk_accel_group_disconnect_key(accel_group_, GDK_Escape,
                                 static_cast<GdkModifierType>(0));
  if (ContainingView() && ContainingView()->GetTopLevelNativeWindow()) {
    gtk_window_remove_accel_group(
        GTK_WINDOW(ContainingView()->GetTopLevelNativeWindow()),
        accel_group_);
  }
  g_object_unref(accel_group_);
}

void ConstrainedWindowGtk::ShowConstrainedWindow() {
  gtk_widget_show_all(border_.get());

  // We collaborate with TabContentsViewGtk and stick ourselves in the
  // TabContentsViewGtk's floating container.
  ContainingView()->AttachConstrainedWindow(this);

  visible_ = true;
}

void ConstrainedWindowGtk::CloseConstrainedWindow() {
  if (visible_)
    ContainingView()->RemoveConstrainedWindow(this);
  delegate_->DeleteDelegate();
  owner_->WillClose(this);

  delete this;
}

TabContentsViewGtk* ConstrainedWindowGtk::ContainingView() {
  return static_cast<TabContentsViewGtk*>(owner_->view());
}

void ConstrainedWindowGtk::ConnectAccelerators() {
  gtk_accel_group_connect(accel_group_,
                          GDK_Escape, static_cast<GdkModifierType>(0),
                          static_cast<GtkAccelFlags>(0),
                          g_cclosure_new(G_CALLBACK(OnEscapeThunk),
                                         this, NULL));
  gtk_window_add_accel_group(
      GTK_WINDOW(ContainingView()->GetTopLevelNativeWindow()),
      accel_group_);
}


gboolean ConstrainedWindowGtk::OnEscape(GtkAccelGroup* group,
                                        GObject* acceleratable,
                                        guint keyval,
                                        GdkModifierType modifier) {
  // Handle this accelerator only if this is on the currently selected tab.
  Browser* browser = BrowserList::GetLastActive();
  if (!browser || browser->GetSelectedTabContents() != owner_)
    return FALSE;

  CloseConstrainedWindow();
  return TRUE;
}

// static
ConstrainedWindow* ConstrainedWindow::CreateConstrainedDialog(
    TabContents* parent,
    ConstrainedWindowGtkDelegate* delegate) {
  return new ConstrainedWindowGtk(parent, delegate);
}

