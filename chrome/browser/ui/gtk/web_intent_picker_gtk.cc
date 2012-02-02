// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/web_intent_picker_gtk.h"

#include <algorithm>

#include "base/bind.h"
#include "base/callback.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/location_bar_view_gtk.h"
#include "chrome/browser/ui/gtk/tab_contents_container_gtk.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "chrome/browser/ui/intents/web_intent_picker_model.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "content/browser/renderer_host/render_view_host.h"
#include "content/browser/renderer_host/render_widget_host_view_gtk.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_view.h"
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

GtkThemeService *GetThemeService(TabContentsWrapper* wrapper) {
  return GtkThemeService::GetFrom(wrapper->profile());
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
      bubble_(NULL),
      browser_(browser) {
  DCHECK(delegate_ != NULL);
  DCHECK(browser);
  DCHECK(browser->window());

  model_->set_observer(this);

  BrowserWindowGtk* browser_window =
      BrowserWindowGtk::GetBrowserWindowForNativeWindow(
          browser->window()->GetNativeHandle());

  GtkWidget* anchor = browser_window->
      GetToolbar()->GetLocationBarView()->location_icon_widget();

  InitContents();

  BubbleGtk::ArrowLocationGtk arrow_location = base::i18n::IsRTL() ?
      BubbleGtk::ARROW_LOCATION_TOP_RIGHT :
      BubbleGtk::ARROW_LOCATION_TOP_LEFT;
  bubble_ = BubbleGtk::Show(anchor,
                            NULL,  // |rect|
                            contents_,
                            arrow_location,
                            true,  // |match_system_theme|
                            true,  // |grab_input|
                            GetThemeService(wrapper),
                            this);  // |delegate|
}

WebIntentPickerGtk::~WebIntentPickerGtk() {
  DCHECK(bubble_ == NULL) << "Should have closed window before destroying.";
}

void WebIntentPickerGtk::OnModelChanged(WebIntentPickerModel* model) {
  gtk_util::RemoveAllChildren(button_vbox_);

  for (size_t i = 0; i < model->GetItemCount(); ++i) {
    const WebIntentPickerModel::Item& item = model->GetItemAt(i);

    GtkWidget* button = gtk_button_new();

    gtk_widget_set_tooltip_text(button, item.url.spec().c_str());
    gtk_button_set_label(GTK_BUTTON(button), UTF16ToUTF8(item.title).c_str());

    gtk_box_pack_start(GTK_BOX(button_vbox_), button, FALSE, FALSE, 0);
    g_signal_connect(button,
                     "clicked",
                     G_CALLBACK(OnServiceButtonClickThunk),
                     this);

    SetServiceButtonImage(button, item.favicon.ToGdkPixbuf());
  }

  gtk_widget_show_all(button_vbox_);
}

void WebIntentPickerGtk::OnFaviconChanged(WebIntentPickerModel* model,
                                          size_t index) {
  const WebIntentPickerModel::Item& item = model->GetItemAt(index);
  GtkWidget* button = GetServiceButton(index);
  SetServiceButtonImage(button, item.favicon.ToGdkPixbuf());
}

void WebIntentPickerGtk::OnInlineDisposition(WebIntentPickerModel* model) {
  const WebIntentPickerModel::Item& item = model->GetItemAt(
      model->inline_disposition_index());
  const GURL& url = item.url;

  content::WebContents* web_contents = content::WebContents::Create(
      browser_->profile(), NULL, MSG_ROUTING_NONE, NULL, NULL);
  inline_disposition_tab_contents_.reset(new TabContentsWrapper(web_contents));
  inline_disposition_delegate_.reset(new InlineDispositionDelegate);
  web_contents->SetDelegate(inline_disposition_delegate_.get());
  tab_contents_container_.reset(new TabContentsContainerGtk(NULL));
  tab_contents_container_->SetTab(inline_disposition_tab_contents_.get());

  inline_disposition_tab_contents_->web_contents()->GetController().LoadURL(
      url, content::Referrer(), content::PAGE_TRANSITION_START_PAGE,
      std::string());

  // Replace the bubble picker contents with the inline disposition.

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

  // TODO(gbillock): This size calculation needs more thought.
  // Move up to WebIntentPicker?
  gfx::Size tab_size = wrapper_->web_contents()->GetView()->GetContainerSize();
  int width = std::max(tab_size.width()/2, kMainContentWidth);
  int height = std::max(tab_size.height()/2, kMainContentWidth);
  gtk_widget_set_size_request(tab_contents_container_->widget(),
                              width, height);
  gtk_widget_show_all(contents_);

  delegate_->OnInlineDispositionWebContentsCreated(web_contents);
}

void WebIntentPickerGtk::Close() {
  bubble_->Close();
  if (inline_disposition_tab_contents_.get())
    inline_disposition_tab_contents_->web_contents()->OnCloseStarted();
}

void WebIntentPickerGtk::BubbleClosing(BubbleGtk* bubble,
                                       bool closed_by_escape) {
  delegate_->OnClosing();
}

void WebIntentPickerGtk::OnCloseButtonClick(GtkWidget* button) {
  delegate_->OnCancelled();
  delegate_->OnClosing();
}

void WebIntentPickerGtk::OnDestroy(GtkWidget* button) {
  // Destroy this object when the BubbleGtk is destroyed. It can't be "delete
  // this" because this function happens in a callback.
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  model_->set_observer(NULL);
  bubble_ = NULL;
}

void WebIntentPickerGtk::OnServiceButtonClick(GtkWidget* button) {
  GList* button_list = gtk_container_get_children(GTK_CONTAINER(button_vbox_));
  gint index = g_list_index(button_list, button);
  DCHECK(index != -1);

  const WebIntentPickerModel::Item& item = model_->GetItemAt(index);

  delegate_->OnServiceChosen(static_cast<size_t>(index), item.disposition);
}

void WebIntentPickerGtk::InitContents() {
  GtkThemeService* theme_service = GetThemeService(wrapper_);

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

// WebIntentPickerGtk::InlineDispositionDelegate

WebIntentPickerGtk::InlineDispositionDelegate::InlineDispositionDelegate() {}
WebIntentPickerGtk::InlineDispositionDelegate::~InlineDispositionDelegate() {}

bool WebIntentPickerGtk::InlineDispositionDelegate::IsPopupOrPanel(
    const WebContents* source) const {
  return true;
}

bool WebIntentPickerGtk::InlineDispositionDelegate::
ShouldAddNavigationToHistory(
    const history::HistoryAddPageArgs& add_page_args,
    content::NavigationType navigation_type) {
  return false;
}
