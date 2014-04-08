// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/web_dialog_gtk.h"

#include <gtk/gtk.h>

#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/webui/chrome_web_contents_handler.h"
#include "content/public/browser/native_web_keyboard_event.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
#include "ui/web_dialogs/web_dialog_ui.h"

using content::NativeWebKeyboardEvent;
using content::WebContents;
using content::WebUIMessageHandler;
using ui::WebDialogDelegate;
using ui::WebDialogUI;
using ui::WebDialogWebContentsDelegate;

namespace chrome {

gfx::NativeWindow ShowWebDialog(gfx::NativeWindow parent,
                                content::BrowserContext* context,
                                WebDialogDelegate* delegate) {
  WebDialogGtk* web_dialog = new WebDialogGtk(context, delegate, parent);
  return web_dialog->InitDialog();
}

}  // namespace chrome

namespace {

void SetDialogStyle() {
  static bool style_was_set = false;

  if (style_was_set)
    return;
  style_was_set = true;

  gtk_rc_parse_string(
      "style \"chrome-html-dialog\" {\n"
      "  GtkDialog::action-area-border = 0\n"
      "  GtkDialog::content-area-border = 0\n"
      "  GtkDialog::content-area-spacing = 0\n"
      "}\n"
      "widget \"*chrome-html-dialog\" style \"chrome-html-dialog\"");
}

}  // namespace

////////////////////////////////////////////////////////////////////////////////
// WebDialogGtk, public:

WebDialogGtk::WebDialogGtk(content::BrowserContext* context,
                           WebDialogDelegate* delegate,
                           gfx::NativeWindow parent_window)
    : WebDialogWebContentsDelegate(context, new ChromeWebContentsHandler),
      delegate_(delegate),
      parent_window_(parent_window),
      dialog_(NULL) {
}

WebDialogGtk::~WebDialogGtk() {
}

////////////////////////////////////////////////////////////////////////////////
// WebDialogDelegate implementation:

ui::ModalType WebDialogGtk::GetDialogModalType() const {
  return delegate_ ? delegate_->GetDialogModalType() : ui::MODAL_TYPE_NONE;
}

base::string16 WebDialogGtk::GetDialogTitle() const {
  return delegate_ ? delegate_->GetDialogTitle() : base::string16();
}

GURL WebDialogGtk::GetDialogContentURL() const {
  if (delegate_)
    return delegate_->GetDialogContentURL();
  else
    return GURL();
}

void WebDialogGtk::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
  if (delegate_)
    delegate_->GetWebUIMessageHandlers(handlers);
  else
    handlers->clear();
}

void WebDialogGtk::GetDialogSize(gfx::Size* size) const {
  if (delegate_)
    delegate_->GetDialogSize(size);
  else
    *size = gfx::Size();
}

void WebDialogGtk::GetMinimumDialogSize(gfx::Size* size) const {
  if (delegate_)
    delegate_->GetMinimumDialogSize(size);
  else
    *size = gfx::Size();
}

std::string WebDialogGtk::GetDialogArgs() const {
  if (delegate_)
    return delegate_->GetDialogArgs();
  else
    return std::string();
}

void WebDialogGtk::OnDialogClosed(const std::string& json_retval) {
  DCHECK(dialog_);

  Detach();
  if (delegate_) {
    WebDialogDelegate* dialog_delegate = delegate_;
    delegate_ = NULL;  // We will not communicate further with the delegate.

    // Store the dialog bounds.
    gfx::Rect dialog_bounds = gtk_util::GetDialogBounds(GTK_WIDGET(dialog_));
    dialog_delegate->StoreDialogSize(dialog_bounds.size());

    dialog_delegate->OnDialogClosed(json_retval);
  }

  gtk_widget_destroy(dialog_);
  delete this;
}

void WebDialogGtk::OnCloseContents(WebContents* source,
                                   bool* out_close_dialog) {
  if (delegate_)
    delegate_->OnCloseContents(source, out_close_dialog);
}

void WebDialogGtk::CloseContents(WebContents* source) {
  DCHECK(dialog_);

  bool close_dialog = false;
  OnCloseContents(source, &close_dialog);
  if (close_dialog)
    OnDialogClosed(std::string());
}

content::WebContents* WebDialogGtk::OpenURLFromTab(
    content::WebContents* source,
    const content::OpenURLParams& params) {
  content::WebContents* new_contents = NULL;
  if (delegate_ &&
      delegate_->HandleOpenURLFromTab(source, params, &new_contents)) {
    return new_contents;
  }
  return WebDialogWebContentsDelegate::OpenURLFromTab(source, params);
}

void WebDialogGtk::AddNewContents(content::WebContents* source,
                                  content::WebContents* new_contents,
                                  WindowOpenDisposition disposition,
                                  const gfx::Rect& initial_pos,
                                  bool user_gesture,
                                  bool* was_blocked) {
  if (delegate_ && delegate_->HandleAddNewContents(
          source, new_contents, disposition, initial_pos, user_gesture)) {
    return;
  }
  WebDialogWebContentsDelegate::AddNewContents(
      source, new_contents, disposition, initial_pos, user_gesture,
      was_blocked);
}

void WebDialogGtk::LoadingStateChanged(content::WebContents* source,
                                       bool to_different_document) {
  if (delegate_)
    delegate_->OnLoadingStateChanged(source);
}

bool WebDialogGtk::ShouldShowDialogTitle() const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// content::WebContentsDelegate implementation:

// A simplified version of BrowserWindowGtk::HandleKeyboardEvent().
// We don't handle global keyboard shortcuts here, but that's fine since
// they're all browser-specific. (This may change in the future.)
void WebDialogGtk::HandleKeyboardEvent(content::WebContents* source,
                                       const NativeWebKeyboardEvent& event) {
  GdkEventKey* os_event = &event.os_event->key;
  if (!os_event || event.type == blink::WebInputEvent::Char)
    return;

  // To make sure the default key bindings can still work, such as Escape to
  // close the dialog.
  gtk_bindings_activate_event(GTK_OBJECT(dialog_), os_event);
}

////////////////////////////////////////////////////////////////////////////////
// WebDialogGtk:

gfx::NativeWindow WebDialogGtk::InitDialog() {
  web_contents_.reset(WebContents::Create(
      WebContents::CreateParams(browser_context())));
  web_contents_->SetDelegate(this);

  // This must be done before loading the page; see the comments in
  // WebDialogUI.
  WebDialogUI::SetDelegate(web_contents_.get(), this);

  web_contents_->GetController().LoadURL(
      GetDialogContentURL(),
      content::Referrer(),
      content::PAGE_TRANSITION_AUTO_TOPLEVEL,
      std::string());
  GtkDialogFlags flags = GTK_DIALOG_NO_SEPARATOR;
  if (delegate_->GetDialogModalType() != ui::MODAL_TYPE_NONE)
    flags = static_cast<GtkDialogFlags>(flags | GTK_DIALOG_MODAL);

  dialog_ = gtk_dialog_new_with_buttons(
      base::UTF16ToUTF8(delegate_->GetDialogTitle()).c_str(),
      parent_window_,
      flags,
      NULL);

  SetDialogStyle();
  gtk_widget_set_name(dialog_, "chrome-html-dialog");
  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponseThunk), this);

  GtkWidget* content_area = gtk_dialog_get_content_area(GTK_DIALOG(dialog_));
  gfx::NativeView web_contents_widget =
      web_contents_->GetView()->GetNativeView();
  gtk_box_pack_start(GTK_BOX(content_area), web_contents_widget, TRUE, TRUE, 0);

  gfx::Size dialog_size;
  delegate_->GetDialogSize(&dialog_size);
  if (!dialog_size.IsEmpty()) {
    gtk_window_set_default_size(GTK_WINDOW(dialog_),
                                dialog_size.width(),
                                dialog_size.height());
  }
  gfx::Size minimum_dialog_size;
  delegate_->GetMinimumDialogSize(&minimum_dialog_size);
  if (!minimum_dialog_size.IsEmpty()) {
    gtk_widget_set_size_request(GTK_WIDGET(web_contents_widget),
                                minimum_dialog_size.width(),
                                minimum_dialog_size.height());
  }

  gtk_widget_show_all(dialog_);

  web_contents_->GetView()->SetInitialFocus();

  return GTK_WINDOW(dialog_);
}

void WebDialogGtk::OnResponse(GtkWidget* dialog, int response_id) {
  OnDialogClosed(std::string());
}
