// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/web_intent_picker_gtk.h"

#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "content/common/content_notification_types.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image/image.h"

namespace {

GtkThemeService *GetThemeService(TabContents* tab_contents) {
  Profile* profile = Profile::FromBrowserContext(
      tab_contents->browser_context());
  return GtkThemeService::GetFrom(profile);
}

} // namespace

// static
WebIntentPicker* WebIntentPicker::Create(TabContents* tab_contents,
                                         WebIntentPickerDelegate* delegate) {
  return new WebIntentPickerGtk(tab_contents, delegate);
}

WebIntentPickerGtk::WebIntentPickerGtk(TabContents* tab_contents,
                                       WebIntentPickerDelegate* delegate)
    : tab_contents_(tab_contents),
      delegate_(delegate),
      window_(NULL) {
  DCHECK(delegate_ != NULL);
  root_.Own(gtk_vbox_new(FALSE, gtk_util::kContentAreaBorder));
  GtkWidget* hbox = gtk_hbox_new(FALSE, 0);
  GtkWidget* label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_CHOOSE_INTENT_HANDLER_MESSAGE).c_str());
  gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);

  close_button_.reset(
      CustomDrawButton::CloseButton(GetThemeService(tab_contents_)));
  g_signal_connect(close_button_->widget(),
                   "clicked",
                   G_CALLBACK(OnCloseButtonClickThunk),
                   this);
  gtk_widget_set_can_focus(close_button_->widget(), FALSE);
  gtk_box_pack_end(GTK_BOX(hbox), close_button_->widget(), FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(root_.get()), hbox, FALSE, FALSE, 0);

  button_hbox_ = gtk_hbox_new(FALSE, gtk_util::kContentAreaBorder);
  gtk_box_pack_start(GTK_BOX(root_.get()), button_hbox_, TRUE, TRUE, 0);
}

WebIntentPickerGtk::~WebIntentPickerGtk() {
  DCHECK(window_ == NULL) << "Should have closed window before destroying.";
}

void WebIntentPickerGtk::SetServiceURLs(const std::vector<GURL>& urls) {
  for (size_t i = 0; i < urls.size(); ++i) {
    GtkWidget* button = gtk_button_new();
    gtk_widget_set_tooltip_text(button, urls[i].spec().c_str());
    gtk_box_pack_start(GTK_BOX(button_hbox_), button, FALSE, FALSE, 0);
    g_signal_connect(button,
                     "clicked",
                     G_CALLBACK(OnServiceButtonClickThunk),
                     this);
  }

  gtk_widget_show_all(button_hbox_);
}

void WebIntentPickerGtk::SetServiceIcon(size_t index, const SkBitmap& icon) {
  if (icon.empty())
    return;

  GList* button_list = gtk_container_get_children(GTK_CONTAINER(button_hbox_));
  GtkButton* button = GTK_BUTTON(g_list_nth_data(button_list, index));
  DCHECK(button != NULL) << "Invalid index.";

  GdkPixbuf* icon_pixbuf = gfx::GdkPixbufFromSkBitmap(&icon);
  gtk_button_set_image(button, gtk_image_new_from_pixbuf(icon_pixbuf));
  g_object_unref(icon_pixbuf);
}

void WebIntentPickerGtk::SetDefaultServiceIcon(size_t index) {
  GList* button_list = gtk_container_get_children(GTK_CONTAINER(button_hbox_));
  GtkButton* button = GTK_BUTTON(g_list_nth_data(button_list, index));
  DCHECK(button != NULL) << "Invalid index.";

  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  GdkPixbuf* icon_pixbuf = rb.GetNativeImageNamed(IDR_DEFAULT_FAVICON);
  gtk_button_set_image(button, gtk_image_new_from_pixbuf(icon_pixbuf));
}

void WebIntentPickerGtk::Show() {
  DCHECK(window_ == NULL) << "Show already called.";
  window_ = new ConstrainedWindowGtk(tab_contents_, this);
}

void WebIntentPickerGtk::Close() {
  DCHECK(window_ != NULL) << "Show not called.";
  window_->CloseConstrainedWindow();
  window_ = NULL;
}

GtkWidget* WebIntentPickerGtk::GetWidgetRoot() {
  return root_.get();
}

GtkWidget* WebIntentPickerGtk::GetFocusWidget() {
  return root_.get();
}

void WebIntentPickerGtk::DeleteDelegate() {
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void WebIntentPickerGtk::OnCloseButtonClick(GtkWidget* button) {
  delegate_->OnCancelled();
}

void WebIntentPickerGtk::OnServiceButtonClick(GtkWidget* button) {
  GList* button_list = gtk_container_get_children(GTK_CONTAINER(button_hbox_));
  gint index = g_list_index(button_list, button);
  DCHECK(index != -1);

  delegate_->OnServiceChosen(static_cast<size_t>(index));
}
