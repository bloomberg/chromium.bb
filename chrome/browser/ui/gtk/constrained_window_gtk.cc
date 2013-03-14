// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/constrained_window_gtk.h"

#include <gdk/gdkkeysyms.h>

#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/tab_contents/chrome_web_contents_view_delegate_gtk.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager_delegate.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/gtk/focus_store_gtk.h"
#include "ui/base/gtk/gtk_compat.h"
#include "ui/base/gtk/gtk_hig_constants.h"

using content::BrowserThread;

ConstrainedWindowGtkDelegate::~ConstrainedWindowGtkDelegate() {
}

bool ConstrainedWindowGtkDelegate::GetBackgroundColor(GdkColor* color) {
  return false;
}

ConstrainedWindowGtk::ConstrainedWindowGtk(
    content::WebContents* web_contents,
    ConstrainedWindowGtkDelegate* delegate)
    : web_contents_(web_contents),
      delegate_(delegate),
      visible_(false) {
  DCHECK(web_contents);
  DCHECK(delegate);
  GtkWidget* dialog = delegate->GetWidgetRoot();

  // Unlike other users of CreateBorderBin, we need a dedicated frame around
  // our "window".
  border_ = gtk_event_box_new();
  g_object_ref_sink(border_);
  GtkWidget* frame = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);

  GtkWidget* alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment),
      ui::kContentAreaBorder, ui::kContentAreaBorder,
      ui::kContentAreaBorder, ui::kContentAreaBorder);

  if (gtk_widget_get_parent(dialog))
    gtk_widget_reparent(dialog, alignment);
  else
    gtk_container_add(GTK_CONTAINER(alignment), dialog);

  gtk_container_add(GTK_CONTAINER(frame), alignment);
  gtk_container_add(GTK_CONTAINER(border_), frame);

  GdkColor background;
  if (delegate_->GetBackgroundColor(&background)) {
    gtk_widget_modify_base(border_, GTK_STATE_NORMAL, &background);
    gtk_widget_modify_fg(border_, GTK_STATE_NORMAL, &background);
    gtk_widget_modify_bg(border_, GTK_STATE_NORMAL, &background);
  }

  gtk_widget_add_events(border_, GDK_KEY_PRESS_MASK);
  g_signal_connect(border_, "key-press-event", G_CALLBACK(OnKeyPressThunk),
                   this);
  g_signal_connect(border_, "hierarchy-changed",
                   G_CALLBACK(OnHierarchyChangedThunk), this);
  g_signal_connect(border_, "destroy", G_CALLBACK(OnDestroyThunk),
                   this);

  // TODO(wittman): Getting/setting data on the widget is a hack to facilitate
  // looking up the ConstrainedWindowGtk from the GtkWindow during refactoring.
  // Remove once ConstrainedWindowGtk is gone.
  g_object_set_data(G_OBJECT(border_), "ConstrainedWindowGtk", this);
}

ConstrainedWindowGtk::~ConstrainedWindowGtk() {
}

void ConstrainedWindowGtk::ShowWebContentsModalDialog() {
  gtk_widget_show_all(border_);

  // We collaborate with WebContentsView and stick ourselves in the
  // WebContentsView's floating container.
  ContainingView()->AttachWebContentsModalDialog(border_);

  visible_ = true;
}

void ConstrainedWindowGtk::FocusWebContentsModalDialog() {
  GtkWidget* focus_widget = delegate_->GetFocusWidget();
  if (!focus_widget)
    return;

  // The user may have focused another tab. In this case do not grab focus
  // until this tab is refocused.
  if (gtk_util::IsWidgetAncestryVisible(focus_widget))
    gtk_widget_grab_focus(focus_widget);
  else
    ContainingView()->focus_store()->SetWidget(focus_widget);
}

void ConstrainedWindowGtk::PulseWebContentsModalDialog() {
}

NativeWebContentsModalDialog ConstrainedWindowGtk::GetNativeDialog() {
  return widget();
}

ConstrainedWindowGtk::TabContentsViewType*
ConstrainedWindowGtk::ContainingView() {
  return ChromeWebContentsViewDelegateGtk::GetFor(web_contents_);
}

gboolean ConstrainedWindowGtk::OnKeyPress(GtkWidget* sender,
                                          GdkEventKey* key) {
  if (key->keyval == GDK_Escape) {
    gtk_widget_destroy(border_);
    return TRUE;
  }

  return FALSE;
}

void ConstrainedWindowGtk::OnHierarchyChanged(GtkWidget* sender,
                                              GtkWidget* previous_toplevel) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));

  if (!gtk_widget_is_toplevel(gtk_widget_get_toplevel(border_)))
    return;

  FocusWebContentsModalDialog();
}

void ConstrainedWindowGtk::OnDestroy(GtkWidget* sender) {
  if (visible_)
    ContainingView()->RemoveWebContentsModalDialog(border_);
  delegate_->DeleteDelegate();
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents_);
  web_contents_modal_dialog_manager->WillClose(border_);

  g_object_unref(border_);
  border_ = NULL;

  delete this;
}

GtkWidget* CreateWebContentsModalDialogGtk(
    content::WebContents* web_contents,
    ConstrainedWindowGtkDelegate* delegate) {
  ConstrainedWindowGtk* window =
      new ConstrainedWindowGtk(web_contents, delegate);
  return window->widget();
}
