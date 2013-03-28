// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/tab_contents/chrome_web_contents_view_delegate_gtk.h"
#include "chrome/browser/ui/native_web_contents_modal_dialog_manager.h"
#include "chrome/browser/ui/web_contents_modal_dialog_manager.h"
#include "content/public/browser/browser_thread.h"
#include "ui/base/gtk/focus_store_gtk.h"

namespace {

// Web contents modal dialog manager implementation for the GTK port. Unlike the
// Win32 system, ConstrainedWindowGtk doesn't draw draggable fake windows and
// instead just centers the dialog. It is thus an order of magnitude simpler.
class NativeWebContentsModalDialogManagerGtk
    : public NativeWebContentsModalDialogManager {
 public:
  NativeWebContentsModalDialogManagerGtk(
      NativeWebContentsModalDialogManagerDelegate* native_delegate)
          : native_delegate_(native_delegate),
            shown_widget_(NULL) {
  }

  virtual ~NativeWebContentsModalDialogManagerGtk() {
  }

  // NativeWebContentsModalDialogManager overrides
  virtual void ManageDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    DCHECK(g_object_is_floating(GetGtkWidget(dialog)));
    g_object_ref_sink(GetGtkWidget(dialog));

    g_signal_connect(GetGtkWidget(dialog), "hierarchy-changed",
                     G_CALLBACK(OnHierarchyChangedThunk), this);
    g_signal_connect(GetGtkWidget(dialog),
                     "destroy",
                     G_CALLBACK(OnDestroyThunk),
                     this);
  }

  virtual void ShowDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    GtkWidget* widget = GetGtkWidget(dialog);

    // Any previously-shown widget should be destroyed before showing a new
    // widget.
    DCHECK(shown_widget_ == widget || shown_widget_ == NULL);
    gtk_widget_show_all(widget);

    if (!shown_widget_) {
      // We collaborate with WebContentsView and stick ourselves in the
      // WebContentsView's floating container.
      ContainingView()->AttachWebContentsModalDialog(widget);
    }

    shown_widget_ = widget;
  }

  virtual void HideDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    gtk_widget_hide(GetGtkWidget(dialog));
  }

  virtual void CloseDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    gtk_widget_destroy(GetGtkWidget(dialog));
  }

  virtual void FocusDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
    GtkWidget* focus_widget =
        reinterpret_cast<GtkWidget*>(
            g_object_get_data(G_OBJECT(GetGtkWidget(dialog)), "focus_widget"));

    if (!focus_widget)
      return;

    // The user may have focused another tab. In this case do not grab focus
    // until this tab is refocused.
    if (gtk_util::IsWidgetAncestryVisible(focus_widget))
      gtk_widget_grab_focus(focus_widget);
    else
      ContainingView()->focus_store()->SetWidget(focus_widget);
  }

  virtual void PulseDialog(NativeWebContentsModalDialog dialog) OVERRIDE {
  }

 private:
  typedef ChromeWebContentsViewDelegateGtk TabContentsViewType;

  GtkWidget* GetGtkWidget(NativeWebContentsModalDialog dialog) {
    return GTK_WIDGET(dialog);
  }

  // Returns the View that we collaborate with to position ourselves.
  TabContentsViewType* ContainingView() const {
    // WebContents may be destroyed already on tab shutdown.
     content::WebContents* web_contents = native_delegate_->GetWebContents();
    return web_contents ?
        ChromeWebContentsViewDelegateGtk::GetFor(web_contents) : NULL;
  }

  CHROMEGTK_CALLBACK_1(NativeWebContentsModalDialogManagerGtk,
                       void,
                       OnHierarchyChanged,
                       GtkWidget*);
  CHROMEGTK_CALLBACK_0(NativeWebContentsModalDialogManagerGtk, void, OnDestroy);

  NativeWebContentsModalDialogManagerDelegate* native_delegate_;

  // The widget currently being shown.
  GtkWidget* shown_widget_;

  DISALLOW_COPY_AND_ASSIGN(NativeWebContentsModalDialogManagerGtk);
};

void NativeWebContentsModalDialogManagerGtk::OnHierarchyChanged(
    GtkWidget* sender,
    GtkWidget* previous_toplevel) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (!gtk_widget_is_toplevel(gtk_widget_get_toplevel(sender)))
    return;

  FocusDialog(sender);
}

void NativeWebContentsModalDialogManagerGtk::OnDestroy(
    GtkWidget* sender) {
  DCHECK(content::BrowserThread::CurrentlyOn(content::BrowserThread::UI));

  if (shown_widget_ == sender) {
    // The containing view may already be destroyed on tab shutdown.
    if (ContainingView())
      ContainingView()->RemoveWebContentsModalDialog(sender);

    shown_widget_ = NULL;
  }

  native_delegate_->WillClose(sender);

  g_object_unref(sender);
}

}  // namespace

NativeWebContentsModalDialogManager*
    WebContentsModalDialogManager::CreateNativeManager(
        NativeWebContentsModalDialogManagerDelegate* native_delegate) {
  return new NativeWebContentsModalDialogManagerGtk(native_delegate);
}
