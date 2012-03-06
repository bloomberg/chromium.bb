// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/web_intent_picker_gtk.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/location_bar_view_gtk.h"
#include "chrome/browser/ui/gtk/tab_contents_container_gtk.h"
#include "chrome/browser/ui/gtk/theme_service_gtk.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "chrome/browser/ui/intents/web_intent_picker_model.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image/image.h"

using content::WebContents;

namespace {

// The width in pixels of the area between the icon on the left and the close
// button on the right.
const int kMainContentWidth = 300;

// The length in pixels of the label at the bottom of the picker. Text longer
// than this width will wrap.
const int kWebStoreLabelLength = 300;

// The pixel size of the label at the bottom of the picker.
const int kWebStoreLabelPixelSize = 11;

ThemeServiceGtk *GetThemeService(TabContentsWrapper* wrapper) {
  return ThemeServiceGtk::GetFrom(wrapper->profile());
}

// Set the image of |button| to |pixbuf|.
void SetServiceButtonImage(GtkWidget* button, GdkPixbuf* pixbuf) {
  gtk_button_set_image(GTK_BUTTON(button), gtk_image_new_from_pixbuf(pixbuf));
  gtk_button_set_image_position(GTK_BUTTON(button), GTK_POS_LEFT);
}

} // namespace

// static
WebIntentPicker* WebIntentPicker::Create(Browser* browser,
                                         TabContentsWrapper* wrapper,
                                         WebIntentPickerDelegate* delegate,
                                         WebIntentPickerModel* model) {
  return new WebIntentPickerGtk(browser, wrapper, delegate, model);
}

WebIntentPickerGtk::WebIntentPickerGtk(Browser* browser,
                                       TabContentsWrapper* wrapper,
                                       WebIntentPickerDelegate* delegate,
                                       WebIntentPickerModel* model)
    : wrapper_(wrapper),
      delegate_(delegate),
      model_(model),
      contents_(NULL),
      button_vbox_(NULL),
      window_(NULL),
      browser_(browser) {
  DCHECK(delegate_ != NULL);
  DCHECK(browser);

  model_->set_observer(this);
  InitContents();
  window_ = new ConstrainedWindowGtk(wrapper, this);
}

WebIntentPickerGtk::~WebIntentPickerGtk() {
}

void WebIntentPickerGtk::Close() {
  window_->CloseConstrainedWindow();
  if (inline_disposition_tab_contents_.get())
    inline_disposition_tab_contents_->web_contents()->OnCloseStarted();
}

void WebIntentPickerGtk::OnModelChanged(WebIntentPickerModel* model) {
  gtk_util::RemoveAllChildren(button_vbox_);

  for (size_t i = 0; i < model->GetInstalledServiceCount(); ++i) {
    const WebIntentPickerModel::InstalledService& installed_service =
        model->GetInstalledServiceAt(i);

    GtkWidget* button = gtk_button_new();

    gtk_widget_set_tooltip_text(button, installed_service.url.spec().c_str());
    gtk_button_set_label(GTK_BUTTON(button),
                         UTF16ToUTF8(installed_service.title).c_str());

    gtk_box_pack_start(GTK_BOX(button_vbox_), button, FALSE, FALSE, 0);
    g_signal_connect(button,
                     "clicked",
                     G_CALLBACK(OnServiceButtonClickThunk),
                     this);

    SetServiceButtonImage(button, installed_service.favicon.ToGdkPixbuf());
  }

  gtk_widget_show_all(button_vbox_);
}

void WebIntentPickerGtk::OnFaviconChanged(WebIntentPickerModel* model,
                                          size_t index) {
  const WebIntentPickerModel::InstalledService& installed_service =
      model->GetInstalledServiceAt(index);
  GtkWidget* button = GetServiceButton(index);
  SetServiceButtonImage(button, installed_service.favicon.ToGdkPixbuf());
}

void WebIntentPickerGtk::OnExtensionIconChanged(WebIntentPickerModel* model,
                                                const string16& extension_id) {
  // TODO(binji): implement.
}

void WebIntentPickerGtk::OnInlineDisposition(WebIntentPickerModel* model) {
  const WebIntentPickerModel::InstalledService& installed_service =
      model->GetInstalledServiceAt(model->inline_disposition_index());
  const GURL& url = installed_service.url;

  content::WebContents* web_contents = content::WebContents::Create(
      browser_->profile(), NULL, MSG_ROUTING_NONE, NULL, NULL);
  inline_disposition_tab_contents_.reset(new TabContentsWrapper(web_contents));
  inline_disposition_delegate_.reset(new WebIntentInlineDispositionDelegate);
  web_contents->SetDelegate(inline_disposition_delegate_.get());

  // Must call this immediately after WebContents creation to avoid race
  // with load.
  delegate_->OnInlineDispositionWebContentsCreated(web_contents);

  tab_contents_container_.reset(new TabContentsContainerGtk(NULL));
  tab_contents_container_->SetTab(inline_disposition_tab_contents_.get());

  inline_disposition_tab_contents_->web_contents()->GetController().LoadURL(
      url, content::Referrer(), content::PAGE_TRANSITION_START_PAGE,
      std::string());

  // Replace the picker contents with the inline disposition.

  gtk_util::RemoveAllChildren(contents_);

  GtkWidget* service_hbox = gtk_hbox_new(FALSE, ui::kControlSpacing);
  // TODO(gbillock): Eventually get the service icon button here.
  // Maybe add a title or something too?
  close_button_.reset(
      CustomDrawButton::CloseButton(GetThemeService(wrapper_)));
  g_signal_connect(close_button_->widget(),
                   "clicked",
                   G_CALLBACK(OnCloseButtonClickThunk),
                   this);
  gtk_widget_set_can_focus(close_button_->widget(), FALSE);
  GtkWidget* close_vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(close_vbox), close_button_->widget(),
                     FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(service_hbox), close_vbox, FALSE, FALSE, 0);

  GtkWidget* vbox = gtk_vbox_new(FALSE, ui::kContentAreaSpacing);
  gtk_box_pack_start(GTK_BOX(vbox), service_hbox, TRUE, TRUE, 0);

  // The separator between the icon/title/close and the inline renderer.
  gtk_box_pack_start(GTK_BOX(vbox), gtk_hseparator_new(), FALSE, TRUE, 0);

  gtk_box_pack_end(GTK_BOX(vbox), tab_contents_container_->widget(),
                   TRUE, TRUE, 0);

  gtk_container_add(GTK_CONTAINER(contents_), vbox);

  gfx::Size size = GetDefaultInlineDispositionSize(web_contents);
  gtk_widget_set_size_request(tab_contents_container_->widget(),
                              size.width(), size.height());
  gtk_widget_show_all(contents_);
}

GtkWidget* WebIntentPickerGtk::GetWidgetRoot() {
  return contents_;
}

GtkWidget* WebIntentPickerGtk::GetFocusWidget() {
  return contents_;
}

void WebIntentPickerGtk::DeleteDelegate() {
  // The delegate is deleted when the contents widget is destroyed. See
  // OnDestroy.
  delegate_->OnClosing();
}

void WebIntentPickerGtk::OnCloseButtonClick(GtkWidget* button) {
  delegate_->OnCancelled();
}

void WebIntentPickerGtk::OnDestroy(GtkWidget* button) {
  // Destroy this object when the contents widget is destroyed. It can't be
  // "delete this" because this function happens in a callback.
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  model_->set_observer(NULL);
  window_ = NULL;
}

void WebIntentPickerGtk::OnServiceButtonClick(GtkWidget* button) {
  GList* button_list = gtk_container_get_children(GTK_CONTAINER(button_vbox_));
  gint index = g_list_index(button_list, button);
  DCHECK(index != -1);

  const WebIntentPickerModel::InstalledService& installed_service =
      model_->GetInstalledServiceAt(index);

  delegate_->OnServiceChosen(static_cast<size_t>(index),
                             installed_service.disposition);
}

void WebIntentPickerGtk::InitContents() {
  ThemeServiceGtk* theme_service = GetThemeService(wrapper_);

  // Main contents vbox.
  contents_ = gtk_vbox_new(FALSE, ui::kContentAreaSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(contents_),
                                 ui::kContentAreaBorder);
  gtk_widget_set_size_request(contents_, kMainContentWidth, -1);

  // Hbox containing label and close button.
  GtkWidget* header_hbox = gtk_hbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(contents_), header_hbox, TRUE, TRUE, 0);

  GtkWidget* label = theme_service->BuildLabel(
      l10n_util::GetStringUTF8(IDS_CHOOSE_INTENT_HANDLER_MESSAGE).c_str(),
      ui::kGdkBlack);
  gtk_box_pack_start(GTK_BOX(header_hbox), label, TRUE, TRUE, 0);
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0);

  close_button_.reset(
      CustomDrawButton::CloseButton(GetThemeService(wrapper_)));
  g_signal_connect(close_button_->widget(),
                   "clicked",
                   G_CALLBACK(OnCloseButtonClickThunk),
                   this);
  gtk_widget_set_can_focus(close_button_->widget(), FALSE);
  gtk_box_pack_end(GTK_BOX(header_hbox), close_button_->widget(),
      FALSE, FALSE, 0);

  // Vbox containing all service buttons.
  button_vbox_ = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(contents_), button_vbox_, TRUE, TRUE, 0);

  // Chrome Web Store label.
  GtkWidget* cws_label = theme_service->BuildLabel(
      l10n_util::GetStringUTF8(IDS_FIND_MORE_INTENT_HANDLER_MESSAGE).c_str(),
      ui::kGdkBlack);
  gtk_box_pack_start(GTK_BOX(contents_), cws_label, TRUE, TRUE, 0);
  gtk_misc_set_alignment(GTK_MISC(cws_label), 0, 0);
  gtk_util::SetLabelWidth(cws_label, kWebStoreLabelLength);
  gtk_util::ForceFontSizePixels(cws_label, kWebStoreLabelPixelSize);

  g_signal_connect(contents_, "destroy", G_CALLBACK(&OnDestroyThunk), this);
}

GtkWidget* WebIntentPickerGtk::GetServiceButton(size_t index) {
  GList* button_list = gtk_container_get_children(GTK_CONTAINER(button_vbox_));
  GtkWidget* button = GTK_WIDGET(g_list_nth_data(button_list, index));
  DCHECK(button != NULL);

  return button;
}
