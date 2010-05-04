// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/native_dialog_window.h"

#include "app/gtk_signal.h"
#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "views/controls/native/native_view_host.h"
#include "views/window/dialog_delegate.h"
#include "views/window/window.h"

namespace {

const int kDialogPadding = 3;

const char kNativeDialogHostWindow[] = "_chromeos_native_dialog_host_window_";

}  // namespace

namespace chromeos {

class NativeDialogHost : public views::View,
                         public views::DialogDelegate {
 public:
  explicit NativeDialogHost(gfx::NativeView native_dialog,
                            const gfx::Size& size,
                            bool resizeable);
  ~NativeDialogHost();

  // views::DialogDelegate implementation:
  virtual bool CanResize() const { return resizeable_; }
  virtual int GetDialogButtons() const { return 0; }
  virtual std::wstring GetWindowTitle() const { return title_; }
  virtual views::View* GetContentsView() { return this; }

 protected:
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

  // The GtkDialog whose vbox will be displayed in this view.
  gfx::NativeView dialog_;

  // NativeViewHost for the dialog's contents.
  views::NativeViewHost* contents_view_;

  std::wstring title_;
  gfx::Size size_;
  bool resizeable_;

  int destroy_signal_id_;

  DISALLOW_IMPLICIT_CONSTRUCTORS(NativeDialogHost);
};

///////////////////////////////////////////////////////////////////////////////
// NativeDialogHost, public:

NativeDialogHost::NativeDialogHost(gfx::NativeView native_dialog,
                                   const gfx::Size& size,
                                   bool resizeable)
    : dialog_(native_dialog),
      contents_view_(NULL),
      size_(size),
      resizeable_(resizeable),
      destroy_signal_id_(0) {
  const char* title = gtk_window_get_title(GTK_WINDOW(dialog_));
  if (title)
    UTF8ToWide(title, strlen(title), &title_);

  destroy_signal_id_ = g_signal_connect(dialog_, "destroy",
      G_CALLBACK(OnDialogDestroyThunk), this);
}

NativeDialogHost::~NativeDialogHost() {
  if (dialog_) {
    // Disconnect the "destroy" signal because we are about to destroy
    // the dialog ourselves and no longer interested in it.
    g_signal_handler_disconnect(G_OBJECT(dialog_), destroy_signal_id_);
    gtk_dialog_response(GTK_DIALOG(dialog_), GTK_RESPONSE_CLOSE);
  }
}

void NativeDialogHost::OnDialogDestroy(GtkWidget* widget) {
  dialog_ = NULL;
  destroy_signal_id_ = 0;
  window()->Close();
}

///////////////////////////////////////////////////////////////////////////////
// NativeDialogHost, views::View overrides:

gfx::Size NativeDialogHost::GetPreferredSize() {
  return size_;
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

  g_object_set_data(G_OBJECT(dialog_), kNativeDialogHostWindow,
      reinterpret_cast<gpointer>(window()->GetNativeWindow()));

  gtk_widget_show_all(contents);

  contents_view_ = new views::NativeViewHost();
  AddChildView(contents_view_);
  contents_view_->Attach(contents);

  // Use gtk's default size if size is not speicified.
  if (size_.IsEmpty()) {
    // Use given width or height if given.
    if (size_.width() || size_.height()) {
      int width = size_.width() == 0 ? -1 : size_.width();
      int height = size_.height() == 0 ? -1 : size_.height();
      gtk_widget_set_size_request(contents, width, height);
    }

    GtkRequisition requsition = { 0 };
    gtk_widget_size_request(contents, &requsition);
    size_.set_width(requsition.width);
    size_.set_height(requsition.height);
  }
}

void ShowNativeDialog(gfx::NativeWindow parent,
                      gfx::NativeView native_dialog,
                      const gfx::Size& dialog_size,
                      bool resizeable) {
  NativeDialogHost* native_dialog_host =
      new NativeDialogHost(native_dialog, dialog_size, resizeable);
  views::Window::CreateChromeWindow(parent, gfx::Rect(), native_dialog_host);
  native_dialog_host->window()->Show();
}

gfx::NativeWindow GetNativeDialogWindow(gfx::NativeView native_dialog) {
  return reinterpret_cast<gfx::NativeWindow>(
      g_object_get_data(G_OBJECT(native_dialog), kNativeDialogHostWindow));
}

}  // namespace chromeos
