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

bool ConstrainedWindowGtkDelegate::ShouldHaveBorderPadding() const {
  return true;
}

ConstrainedWindowGtk::ConstrainedWindowGtk(
    content::WebContents* web_contents,
    ConstrainedWindowGtkDelegate* delegate)
    : web_contents_(web_contents),
      delegate_(delegate),
      visible_(false),
      weak_factory_(this) {
  DCHECK(web_contents);
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

  if (gtk_widget_get_parent(dialog))
    gtk_widget_reparent(dialog, alignment);
  else
    gtk_container_add(GTK_CONTAINER(alignment), dialog);

  gtk_container_add(GTK_CONTAINER(frame), alignment);
  gtk_container_add(GTK_CONTAINER(ebox), frame);
  border_.Own(ebox);

  BackgroundColorChanged();

  gtk_widget_add_events(widget(), GDK_KEY_PRESS_MASK);
  g_signal_connect(widget(), "key-press-event", G_CALLBACK(OnKeyPressThunk),
                   this);
  g_signal_connect(widget(), "hierarchy-changed",
                   G_CALLBACK(OnHierarchyChangedThunk), this);

  // TODO(wittman): Getting/setting data on the widget is a hack to facilitate
  // looking up the ConstrainedWindowGtk from the GtkWindow during refactoring.
  // Remove once ConstrainedWindowGtk is gone.
  g_object_set_data(G_OBJECT(widget()), "ConstrainedWindowGtk", this);

  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents_);
  web_contents_modal_dialog_manager->ShowDialog(widget());
}

ConstrainedWindowGtk::~ConstrainedWindowGtk() {
  border_.Destroy();
}

void ConstrainedWindowGtk::ShowWebContentsModalDialog() {
  gtk_widget_show_all(border_.get());

  // We collaborate with WebContentsView and stick ourselves in the
  // WebContentsView's floating container.
  ContainingView()->AttachConstrainedWindow(this);

  visible_ = true;
}

void ConstrainedWindowGtk::CloseWebContentsModalDialog() {
  if (visible_)
    ContainingView()->RemoveConstrainedWindow(this);
  delegate_->DeleteDelegate();
  WebContentsModalDialogManager* web_contents_modal_dialog_manager =
      WebContentsModalDialogManager::FromWebContents(web_contents_);
  web_contents_modal_dialog_manager->WillClose(widget());

  delete this;
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

void ConstrainedWindowGtk::BackgroundColorChanged() {
  GdkColor background;
  if (delegate_->GetBackgroundColor(&background)) {
    gtk_widget_modify_base(border_.get(), GTK_STATE_NORMAL, &background);
    gtk_widget_modify_fg(border_.get(), GTK_STATE_NORMAL, &background);
    gtk_widget_modify_bg(border_.get(), GTK_STATE_NORMAL, &background);
  }
}

ConstrainedWindowGtk::TabContentsViewType*
ConstrainedWindowGtk::ContainingView() {
  return ChromeWebContentsViewDelegateGtk::GetFor(web_contents_);
}

gboolean ConstrainedWindowGtk::OnKeyPress(GtkWidget* sender,
                                          GdkEventKey* key) {
  if (key->keyval == GDK_Escape) {
    // Let the stack unwind so the event handler can release its ref
    // on widget().
    MessageLoop::current()->PostTask(
        FROM_HERE,
        base::Bind(&ConstrainedWindowGtk::CloseWebContentsModalDialog,
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

  FocusWebContentsModalDialog();
}
