// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/html_dialog_gtk.h"

#include <gtk/gtk.h>

#include "base/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_dialogs.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/tab_contents_container_gtk.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/browser/ui/webui/html_dialog_ui.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/public/browser/native_web_keyboard_event.h"

namespace browser {

gfx::NativeWindow ShowHtmlDialog(gfx::NativeWindow parent, Profile* profile,
                                 HtmlDialogUIDelegate* delegate,
                                 DialogStyle style) {
  // It's not always safe to display an html dialog with an off the record
  // profile.  If the last browser with that profile is closed it will go
  // away.
  // Ignore style for now. The style parameter only used in the implementation
  // in html_dialog_view.cc file.
  // TODO (bshe): Add style parameter to HtmlDialogGtk.
  DCHECK(!profile->IsOffTheRecord() || delegate->IsDialogModal());
  HtmlDialogGtk* html_dialog =
      new HtmlDialogGtk(profile, delegate, parent);
  return html_dialog->InitDialog();
}

} // namespace browser

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
// HtmlDialogGtk, public:

HtmlDialogGtk::HtmlDialogGtk(Profile* profile,
                             HtmlDialogUIDelegate* delegate,
                             gfx::NativeWindow parent_window)
    : HtmlDialogTabContentsDelegate(profile),
      delegate_(delegate),
      parent_window_(parent_window),
      dialog_(NULL) {
}

HtmlDialogGtk::~HtmlDialogGtk() {
}

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogUIDelegate implementation:

bool HtmlDialogGtk::IsDialogModal() const {
  return delegate_ ? delegate_->IsDialogModal() : false;
}

string16 HtmlDialogGtk::GetDialogTitle() const {
  return delegate_ ? delegate_->GetDialogTitle() : string16();
}

GURL HtmlDialogGtk::GetDialogContentURL() const {
  if (delegate_)
    return delegate_->GetDialogContentURL();
  else
    return GURL();
}

void HtmlDialogGtk::GetWebUIMessageHandlers(
    std::vector<WebUIMessageHandler*>* handlers) const {
  if (delegate_)
    delegate_->GetWebUIMessageHandlers(handlers);
  else
    handlers->clear();
}

void HtmlDialogGtk::GetDialogSize(gfx::Size* size) const {
  if (delegate_)
    delegate_->GetDialogSize(size);
  else
    *size = gfx::Size();
}

std::string HtmlDialogGtk::GetDialogArgs() const {
  if (delegate_)
    return delegate_->GetDialogArgs();
  else
    return std::string();
}

void HtmlDialogGtk::OnDialogClosed(const std::string& json_retval) {
  DCHECK(dialog_);

  Detach();
  if (delegate_) {
    HtmlDialogUIDelegate* dialog_delegate = delegate_;
    delegate_ = NULL;  // We will not communicate further with the delegate.
    dialog_delegate->OnDialogClosed(json_retval);
  }
  gtk_widget_destroy(dialog_);
  delete this;
}

void HtmlDialogGtk::OnCloseContents(TabContents* source,
                                    bool* out_close_dialog) {
  if (delegate_)
    delegate_->OnCloseContents(source, out_close_dialog);
}

void HtmlDialogGtk::CloseContents(TabContents* source) {
  DCHECK(dialog_);

  bool close_dialog = false;
  OnCloseContents(source, &close_dialog);
  if (close_dialog)
    OnDialogClosed(std::string());
}

bool HtmlDialogGtk::ShouldShowDialogTitle() const {
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// TabContentsDelegate implementation:

// A simplified version of BrowserWindowGtk::HandleKeyboardEvent().
// We don't handle global keyboard shortcuts here, but that's fine since
// they're all browser-specific. (This may change in the future.)
void HtmlDialogGtk::HandleKeyboardEvent(const NativeWebKeyboardEvent& event) {
  GdkEventKey* os_event = &event.os_event->key;
  if (!os_event || event.type == WebKit::WebInputEvent::Char)
    return;

  // To make sure the default key bindings can still work, such as Escape to
  // close the dialog.
  gtk_bindings_activate_event(GTK_OBJECT(dialog_), os_event);
}

////////////////////////////////////////////////////////////////////////////////
// HtmlDialogGtk:

gfx::NativeWindow HtmlDialogGtk::InitDialog() {
  tab_.reset(new TabContentsWrapper(
      new TabContents(profile(), NULL, MSG_ROUTING_NONE, NULL, NULL)));
  tab_->tab_contents()->set_delegate(this);

  // This must be done before loading the page; see the comments in
  // HtmlDialogUI.
  HtmlDialogUI::GetPropertyAccessor().SetProperty(
      tab_->tab_contents()->property_bag(), this);

  tab_->controller().LoadURL(GetDialogContentURL(),
                             GURL(), content::PAGE_TRANSITION_START_PAGE,
                             std::string());
  GtkDialogFlags flags = GTK_DIALOG_NO_SEPARATOR;
  if (delegate_->IsDialogModal())
    flags = static_cast<GtkDialogFlags>(flags | GTK_DIALOG_MODAL);

  dialog_ = gtk_dialog_new_with_buttons(
      UTF16ToUTF8(delegate_->GetDialogTitle()).c_str(),
      parent_window_,
      flags,
      NULL);

  SetDialogStyle();
  gtk_widget_set_name(dialog_, "chrome-html-dialog");
  g_signal_connect(dialog_, "response", G_CALLBACK(OnResponseThunk), this);

  tab_contents_container_.reset(new TabContentsContainerGtk(NULL));
  gtk_box_pack_start(GTK_BOX(GTK_DIALOG(dialog_)->vbox),
                     tab_contents_container_->widget(), TRUE, TRUE, 0);

  tab_contents_container_->SetTab(tab_.get());

  gfx::Size dialog_size;
  delegate_->GetDialogSize(&dialog_size);

  gtk_widget_set_size_request(GTK_WIDGET(tab_contents_container_->widget()),
                              dialog_size.width(),
                              dialog_size.height());

  gtk_widget_show_all(dialog_);

  return GTK_WINDOW(dialog_);
}

void HtmlDialogGtk::OnResponse(GtkWidget* dialog, int response_id) {
  OnDialogClosed(std::string());
}
