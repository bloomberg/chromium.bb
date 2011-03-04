// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/native_dialog_window.h"

#include <gtk/gtk.h>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/chromeos/frame/bubble_window.h"
#include "chrome/browser/ui/views/window.h"
#include "ui/base/gtk/gtk_signal.h"
#include "views/controls/native/native_view_host.h"
#include "views/window/dialog_delegate.h"
#include "views/window/non_client_view.h"
#include "views/window/window.h"

namespace {

const int kDialogPadding = 3;

const char kNativeDialogHost[] = "_chromeos_native_dialog_host_";

// TODO(xiyuan): Use gtk_window_get_default_widget with GTK 2.14+.
// Gets the default widget of given dialog.
GtkWidget* GetDialogDefaultWidget(GtkDialog* dialog) {
  GtkWidget* default_widget = NULL;

  GList* children = gtk_container_get_children(
      GTK_CONTAINER(dialog->action_area));

  GList* current = children;
  while (current) {
    GtkWidget* widget = reinterpret_cast<GtkWidget*>(current->data);
    if (GTK_WIDGET_HAS_DEFAULT(widget)) {
      default_widget = widget;
      break;
    }

    current = g_list_next(current);
  }

  g_list_free(children);

  return default_widget;
}

}  // namespace

namespace chromeos {

class NativeDialogHost : public views::View,
                         public views::DialogDelegate {
 public:
  NativeDialogHost(gfx::NativeView native_dialog,
                   int flags,
                   const gfx::Size& size,
                   const gfx::Size& min_size);
  ~NativeDialogHost();

  // views::DialogDelegate implementation:
  virtual bool CanResize() const { return flags_ & DIALOG_FLAG_RESIZEABLE; }
  virtual int GetDialogButtons() const { return 0; }
  virtual std::wstring GetWindowTitle() const { return title_; }
  virtual views::View* GetContentsView() { return this; }
  virtual bool IsModal() const { return flags_ & DIALOG_FLAG_MODAL; }
  virtual void WindowClosing();

 protected:
  CHROMEGTK_CALLBACK_0(NativeDialogHost, void, OnCheckResize);
  CHROMEGTK_CALLBACK_0(NativeDialogHost, void, OnDialogDestroy);

  // views::View implementation:
  virtual gfx::Size GetPreferredSize();
  virtual void Layout();
  virtual void ViewHierarchyChanged(bool is_add,
                                    views::View* parent,
                                    views::View* child);
 private:
  // Init and attach to native dialog.
  void Init();

  // Check and apply minimum size restriction.
  void CheckSize();

  // The GtkDialog whose vbox will be displayed in this view.
  gfx::NativeView dialog_;

  // NativeViewHost for the dialog's contents.
  views::NativeViewHost* contents_view_;

  std::wstring title_;
  int flags_;
  gfx::Size size_;
  gfx::Size preferred_size_;
  gfx::Size min_size_;

  int destroy_signal_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(NativeDialogHost);
};

///////////////////////////////////////////////////////////////////////////////
// NativeDialogHost, public:

NativeDialogHost::NativeDialogHost(gfx::NativeView native_dialog,
                                   int flags,
                                   const gfx::Size& size,
                                   const gfx::Size& min_size)
    : dialog_(native_dialog),
      contents_view_(NULL),
      flags_(flags),
      size_(size),
      preferred_size_(size),
      min_size_(min_size),
      destroy_signal_id_(0) {
  const char* title = gtk_window_get_title(GTK_WINDOW(dialog_));
  if (title)
    UTF8ToWide(title, strlen(title), &title_);

  destroy_signal_id_ = g_signal_connect(dialog_, "destroy",
      G_CALLBACK(&OnDialogDestroyThunk), this);
}

NativeDialogHost::~NativeDialogHost() {
}

void NativeDialogHost::OnCheckResize(GtkWidget* widget) {
  // Do auto height resize only when we are asked to do so.
  if (size_.height() == 0) {
    gfx::NativeView contents = contents_view_->native_view();

    // Check whether preferred height has changed. We keep the current width
    // unchanged and pass "-1" as height to let gtk calculate a proper height.
    gtk_widget_set_size_request(contents, width(), -1);
    GtkRequisition requsition = { 0 };
    gtk_widget_size_request(contents, &requsition);

    if (preferred_size_.height() != requsition.height) {
      preferred_size_.set_width(requsition.width);
      preferred_size_.set_height(requsition.height);
      CheckSize();
      SizeToPreferredSize();

      gfx::Size window_size = window()->GetNonClientView()->GetPreferredSize();
      gfx::Rect window_bounds = window()->GetBounds();
      window_bounds.set_width(window_size.width());
      window_bounds.set_height(window_size.height());
      window()->SetWindowBounds(window_bounds, NULL);
    }
  }
}

void NativeDialogHost::OnDialogDestroy(GtkWidget* widget) {
  dialog_ = NULL;
  destroy_signal_id_ = 0;
  window()->Close();
}

///////////////////////////////////////////////////////////////////////////////
// NativeDialogHost, views::DialogDelegate implementation:
void NativeDialogHost::WindowClosing() {
  if (dialog_) {
    // Disconnect the "destroy" signal because we are about to destroy
    // the dialog ourselves and no longer interested in it.
    g_signal_handler_disconnect(G_OBJECT(dialog_), destroy_signal_id_);
    gtk_dialog_response(GTK_DIALOG(dialog_), GTK_RESPONSE_DELETE_EVENT);
  }
}

///////////////////////////////////////////////////////////////////////////////
// NativeDialogHost, views::View implementation:

gfx::Size NativeDialogHost::GetPreferredSize() {
  return preferred_size_;
}

void NativeDialogHost::Layout() {
  contents_view_->SetBounds(0, 0, width(), height());
}

void NativeDialogHost::ViewHierarchyChanged(bool is_add,
                                            views::View* parent,
                                            views::View* child) {
  if (is_add && child == this)
    Init();
}

///////////////////////////////////////////////////////////////////////////////
// NativeDialogHost, private:
void NativeDialogHost::Init() {
  if (contents_view_)
    return;

  // Get default widget of the dialog.
  GtkWidget* default_widget = GetDialogDefaultWidget(GTK_DIALOG(dialog_));

  // Get focus widget of the dialog.
  GtkWidget* focus_widget = gtk_window_get_focus(GTK_WINDOW(dialog_));

  // Create a GtkAlignment as dialog contents container.
  GtkWidget* contents = gtk_alignment_new(0.5, 0.5, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(contents),
      kDialogPadding, kDialogPadding,
      kDialogPadding, kDialogPadding);

  // Move dialog contents into our container.
  GtkWidget* dialog_contents = GTK_DIALOG(dialog_)->vbox;
  g_object_ref(dialog_contents);
  gtk_container_remove(GTK_CONTAINER(dialog_), dialog_contents);
  gtk_container_add(GTK_CONTAINER(contents), dialog_contents);
  g_object_unref(dialog_contents);
  gtk_widget_hide(dialog_);

  g_object_set_data(G_OBJECT(dialog_), kNativeDialogHost,
      reinterpret_cast<gpointer>(this));

  gtk_widget_show_all(contents);

  contents_view_ = new views::NativeViewHost();
  // TODO(xiyuan): Find a better way to get proper background.
  contents_view_->set_background(views::Background::CreateSolidBackground(
      BubbleWindow::kBackgroundColor));
  AddChildView(contents_view_);
  contents_view_->Attach(contents);

  g_signal_connect(window()->GetNativeWindow(), "check-resize",
      G_CALLBACK(&OnCheckResizeThunk), this);

  const int padding = 2 * kDialogPadding;
  // Use gtk's default size if size is not specified.
  if (size_.IsEmpty()) {
    // Use given width or height if given.
    if (size_.width() || size_.height()) {
      int width = size_.width() == 0 ? -1 : size_.width() + padding;
      int height = size_.height() == 0 ? -1 : size_.height() + padding;
      gtk_widget_set_size_request(contents, width, height);
    }

    GtkRequisition requsition = { 0 };
    gtk_widget_size_request(contents, &requsition);
    preferred_size_.set_width(requsition.width);
    preferred_size_.set_height(requsition.height);
  } else {
    preferred_size_.set_width(size_.width() + padding);
    preferred_size_.set_height(size_.height() + padding);
  }

  CheckSize();

  if (default_widget)
    gtk_widget_grab_default(default_widget);

  if (focus_widget)
    gtk_widget_grab_focus(focus_widget);
}

void NativeDialogHost::CheckSize() {
  // Apply the minimum size.
  if (preferred_size_.width() < min_size_.width())
    preferred_size_.set_width(min_size_.width());
  if (preferred_size_.height() < min_size_.height())
    preferred_size_.set_height(min_size_.height());
}

void ShowNativeDialog(gfx::NativeWindow parent,
                      gfx::NativeView native_dialog,
                      int flags,
                      const gfx::Size& size,
                      const gfx::Size& min_size) {
  NativeDialogHost* native_dialog_host =
      new NativeDialogHost(native_dialog, flags, size, min_size);
  browser::CreateViewsWindow(parent, gfx::Rect(), native_dialog_host);
  native_dialog_host->window()->Show();
}

gfx::NativeWindow GetNativeDialogWindow(gfx::NativeView native_dialog) {
  NativeDialogHost* host = reinterpret_cast<NativeDialogHost*>(
      g_object_get_data(G_OBJECT(native_dialog), kNativeDialogHost));
  return host ? host->window()->GetNativeWindow() : NULL;
}

gfx::Rect GetNativeDialogContentsBounds(gfx::NativeView native_dialog) {
  NativeDialogHost* host = reinterpret_cast<NativeDialogHost*>(
      g_object_get_data(G_OBJECT(native_dialog), kNativeDialogHost));
  return host ? host->bounds() : gfx::Rect();
}

}  // namespace chromeos
