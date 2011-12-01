// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/constrained_window_gtk.h"

#include <gdk/gdkkeysyms.h>

#include "base/bind.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/constrained_window_tab_helper.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/gtk/gtk_compat.h"
#include "ui/base/gtk/gtk_hig_constants.h"

#if defined(TOOLKIT_VIEWS)
#include "chrome/browser/ui/views/tab_contents/native_tab_contents_view_gtk.h"
#include "chrome/browser/ui/views/tab_contents/tab_contents_view_views.h"
#else
#include "chrome/browser/tab_contents/chrome_tab_contents_view_wrapper_gtk.h"
#include "chrome/browser/tab_contents/tab_contents_view_gtk.h"
#endif

using content::BrowserThread;

ConstrainedWindowGtkDelegate::~ConstrainedWindowGtkDelegate() {
}

bool ConstrainedWindowGtkDelegate::GetBackgroundColor(GdkColor* color) {
  return false;
}

bool ConstrainedWindowGtkDelegate::ShouldHaveBorderPadding() const {
  return true;
}

ConstrainedWindowGtk::ConstrainedWindowGtk(
    TabContentsWrapper* wrapper, ConstrainedWindowGtkDelegate* delegate)
    : wrapper_(wrapper),
      delegate_(delegate),
      visible_(false),
      weak_factory_(this) {
  DCHECK(wrapper);
  DCHECK(delegate);
  GtkWidget* dialog = delegate->GetWidgetRoot();

  // Unlike other users of CreateBorderBin, we need a dedicated frame around
  // our "window".
  GtkWidget* ebox = gtk_event_box_new();
  GtkWidget* frame = gtk_frame_new(NULL);
  gtk_frame_set_shadow_type(GTK_FRAME(frame), GTK_SHADOW_OUT);

  GtkWidget* alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  if (delegate->ShouldHaveBorderPadding()) {
    gtk_alignment_set_padding(GTK_ALIGNMENT(alignment),
        ui::kContentAreaBorder, ui::kContentAreaBorder,
        ui::kContentAreaBorder, ui::kContentAreaBorder);
  }

  GdkColor background;
  if (delegate->GetBackgroundColor(&background)) {
    gtk_widget_modify_base(ebox, GTK_STATE_NORMAL, &background);
    gtk_widget_modify_fg(ebox, GTK_STATE_NORMAL, &background);
    gtk_widget_modify_bg(ebox, GTK_STATE_NORMAL, &background);
  }

  if (gtk_widget_get_parent(dialog))
    gtk_widget_reparent(dialog, alignment);
  else
    gtk_container_add(GTK_CONTAINER(alignment), dialog);

  gtk_container_add(GTK_CONTAINER(frame), alignment);
  gtk_container_add(GTK_CONTAINER(ebox), frame);
  border_.Own(ebox);

  gtk_widget_add_events(widget(), GDK_KEY_PRESS_MASK);
  g_signal_connect(widget(), "key-press-event", G_CALLBACK(OnKeyPressThunk),
                   this);
  g_signal_connect(widget(), "hierarchy-changed",
                   G_CALLBACK(OnHierarchyChangedThunk), this);

  wrapper_->constrained_window_tab_helper()->AddConstrainedDialog(this);
}

ConstrainedWindowGtk::~ConstrainedWindowGtk() {
  border_.Destroy();
}

void ConstrainedWindowGtk::ShowConstrainedWindow() {
  gtk_widget_show_all(border_.get());

  // We collaborate with TabContentsView and stick ourselves in the
  // TabContentsView's floating container.
  ContainingView()->AttachConstrainedWindow(this);

  visible_ = true;
}

void ConstrainedWindowGtk::CloseConstrainedWindow() {
  if (visible_)
    ContainingView()->RemoveConstrainedWindow(this);
  delegate_->DeleteDelegate();
  wrapper_->constrained_window_tab_helper()->WillClose(this);

  delete this;
}

void ConstrainedWindowGtk::FocusConstrainedWindow() {
  GtkWidget* focus_widget = delegate_->GetFocusWidget();
  if (!focus_widget)
    return;

  // The user may have focused another tab. In this case do not grab focus
  // until this tab is refocused.
  ConstrainedWindowTabHelper* helper =
      wrapper_->constrained_window_tab_helper();
  if ((!helper->delegate() ||
       helper->delegate()->ShouldFocusConstrainedWindow()) &&
      gtk_util::IsWidgetAncestryVisible(focus_widget)) {
    gtk_widget_grab_focus(focus_widget);
  } else {
  // TODO(estade): this define should not need to be here because this class
  // should not be used on linux/views.
#if defined(TOOLKIT_GTK)
    static_cast<TabContentsViewGtk*>(wrapper_->view())->
        SetFocusedWidget(focus_widget);
#endif
  }
}

ConstrainedWindowGtk::TabContentsViewType*
    ConstrainedWindowGtk::ContainingView() {
#if defined(TOOLKIT_VIEWS)
  return static_cast<NativeTabContentsViewGtk*>(
      static_cast<TabContentsViewViews*>(wrapper_->view())->
          native_tab_contents_view());
#else
  return static_cast<TabContentsViewType*>(
      static_cast<TabContentsViewGtk*>(wrapper_->view())->
          wrapper());
#endif
}

gboolean ConstrainedWindowGtk::OnKeyPress(GtkWidget* sender,
                                          GdkEventKey* key) {
  if (key->keyval == GDK_Escape) {
    // Let the stack unwind so the event handler can release its ref
    // on widget().
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ConstrainedWindowGtk::CloseConstrainedWindow,
                   weak_factory_.GetWeakPtr()));
    return TRUE;
  }

  return FALSE;
}

void ConstrainedWindowGtk::OnHierarchyChanged(GtkWidget* sender,
                                              GtkWidget* previous_toplevel) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::UI));
  if (!gtk_widget_is_toplevel(gtk_widget_get_toplevel(widget())))
    return;

  FocusConstrainedWindow();
}
