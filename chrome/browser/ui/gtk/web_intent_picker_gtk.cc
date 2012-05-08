// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/web_intent_picker_gtk.h"

#include <algorithm>

#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/favicon/favicon_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/tab_util.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/location_bar_view_gtk.h"
#include "chrome/browser/ui/gtk/tab_contents_container_gtk.h"
#include "chrome/browser/ui/gtk/throbber_gtk.h"
#include "chrome/browser/ui/intents/web_intent_picker_controller.h"
#include "chrome/browser/ui/intents/web_intent_picker_delegate.h"
#include "chrome/browser/ui/intents/web_intent_picker_model.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/notification_types.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/render_widget_host_view.h"
#include "content/public/browser/web_contents.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/google_chrome_strings.h"
#include "grit/theme_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/base/text/text_elider.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image/image.h"

using content::WebContents;

namespace {

// The width in pixels of the area between the icon on the left and the close
// button on the right.
const int kMainContentWidth = 400;

// The pixel size of the header label when using a non-native theme.
const int kHeaderLabelPixelSize = 15;

// The pixel size of the font of the main content of the dialog.
const int kMainContentPixelSize = 13;

// The maximum width in pixels of a suggested extension's title link.
const int kTitleLinkMaxWidth = 130;

// Indices of the extension row widgets.
enum {
  kIconIndex,
  kTitleLinkIndex,
  kStarsIndex,
  kInstallButtonIndex,
};

GtkThemeService *GetThemeService(TabContentsWrapper* wrapper) {
  return GtkThemeService::GetFrom(wrapper->profile());
}

// Set the image of |button| to |pixbuf|.
void SetServiceButtonImage(GtkWidget* button, GdkPixbuf* pixbuf) {
  gtk_button_set_image(GTK_BUTTON(button), gtk_image_new_from_pixbuf(pixbuf));
  gtk_button_set_image_position(GTK_BUTTON(button), GTK_POS_LEFT);
}

void SetWidgetFontSizeCallback(GtkWidget* widget, gpointer data) {
  if (GTK_IS_LABEL(widget)) {
    int* size = static_cast<int*>(data);
    gtk_util::ForceFontSizePixels(widget, *size);
    return;
  }

  if (GTK_IS_CONTAINER(widget))
    gtk_container_forall(GTK_CONTAINER(widget), SetWidgetFontSizeCallback,
                         data);
}

void SetWidgetFontSize(GtkWidget* widget, int size) {
  gtk_container_forall(GTK_CONTAINER(widget), SetWidgetFontSizeCallback, &size);
}

// Get the index of the row containing |widget|. Assume the widget is the child
// of an hbox, which is a child of a vbox. The hbox represents a row, and the
// vbox the full table.
size_t GetExtensionWidgetRow(GtkWidget* widget) {
  GtkWidget* hbox = gtk_widget_get_parent(widget);
  DCHECK(hbox);
  GtkWidget* vbox = gtk_widget_get_parent(hbox);
  DCHECK(vbox);
  GList* hbox_list = gtk_container_get_children(GTK_CONTAINER(vbox));
  gint index = g_list_index(hbox_list, hbox);
  DCHECK(index != -1);
  g_list_free(hbox_list);

  return index;
}

// A gtk_container_foreach callback to enable/disable a widget.
void EnableWidgetCallback(GtkWidget* widget, gpointer data) {
  gtk_widget_set_sensitive(widget, *static_cast<gboolean*>(data));
}

// Create a new widget displaying |rating| as |kNumStarsPerRating| star images.
// Rating should be in the range [0, kNumStarsPerRating].
GtkWidget* CreateStarsWidget(double rating) {
  const int kNumStarsPerRating = 5;  // Number of stars in a rating.
  const int kStarSpacing = 1;  // Spacing between stars in pixels.
  GtkWidget* hbox = gtk_hbox_new(FALSE, kStarSpacing);

  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  for (int i = 0; i < kNumStarsPerRating; ++i) {
    GdkPixbuf* star = rb.GetNativeImageNamed(
        WebIntentPicker::GetNthStarImageIdFromCWSRating(rating, i),
        ui::ResourceBundle::RTL_ENABLED).ToGdkPixbuf();
    gtk_box_pack_start(GTK_BOX(hbox), gtk_image_new_from_pixbuf(star),
                       FALSE, FALSE, 0);
  }

  return hbox;
}

} // namespace

// static
WebIntentPicker* WebIntentPicker::Create(TabContentsWrapper* wrapper,
                                         WebIntentPickerDelegate* delegate,
                                         WebIntentPickerModel* model) {
  return new WebIntentPickerGtk(wrapper, delegate, model);
}

WebIntentPickerGtk::WebIntentPickerGtk(TabContentsWrapper* wrapper,
                                       WebIntentPickerDelegate* delegate,
                                       WebIntentPickerModel* model)
    : wrapper_(wrapper),
      delegate_(delegate),
      model_(model),
      contents_(NULL),
      header_label_(NULL),
      button_vbox_(NULL),
      cws_label_(NULL),
      extensions_vbox_(NULL),
      window_(NULL) {
  DCHECK(delegate_ != NULL);

  model_->set_observer(this);
  InitContents();
  UpdateInstalledServices();
  UpdateCWSLabel();
  UpdateSuggestedExtensions();

  GtkThemeService* theme_service = GetThemeService(wrapper);
  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                       content::Source<ThemeService>(theme_service));
  theme_service->InitThemesFor(this);

  window_ = new ConstrainedWindowGtk(wrapper, this);
}

WebIntentPickerGtk::~WebIntentPickerGtk() {
}

void WebIntentPickerGtk::Close() {
  window_->CloseConstrainedWindow();
  if (inline_disposition_tab_contents_.get())
    inline_disposition_tab_contents_->web_contents()->OnCloseStarted();
}

void WebIntentPickerGtk::SetActionString(const string16& action) {
  gtk_label_set_text(GTK_LABEL(header_label_), UTF16ToUTF8(action).c_str());
}

void WebIntentPickerGtk::OnExtensionInstallSuccess(const std::string& id) {
  RemoveThrobber();
}

void WebIntentPickerGtk::OnExtensionInstallFailure(const std::string& id) {
  // The throbber has an alignment as its parent, so it must be used instead of
  // the throbber to find the extension row.
  size_t index =
      GetExtensionWidgetRow(gtk_widget_get_parent(throbber_->widget()));
  GList* vbox_list =
      gtk_container_get_children(GTK_CONTAINER(extensions_vbox_));
  GtkWidget* hbox = static_cast<GtkWidget*>(g_list_nth_data(vbox_list, index));

  RemoveThrobber();
  gtk_widget_show_all(hbox);
  g_list_free(vbox_list);
  SetWidgetsEnabled(true);
}

void WebIntentPickerGtk::OnModelChanged(WebIntentPickerModel* model) {
  UpdateInstalledServices();
  UpdateCWSLabel();
  UpdateSuggestedExtensions();
}

void WebIntentPickerGtk::OnFaviconChanged(WebIntentPickerModel* model,
                                          size_t index) {
  UpdateInstalledServices();
}

void WebIntentPickerGtk::OnExtensionIconChanged(WebIntentPickerModel* model,
                                                const string16& extension_id) {
  UpdateSuggestedExtensions();
}

void WebIntentPickerGtk::OnInlineDisposition(WebIntentPickerModel* model,
                                             const GURL& url) {
  content::WebContents* web_contents = content::WebContents::Create(
      wrapper_->profile(),
      tab_util::GetSiteInstanceForNewTab(wrapper_->profile(), url),
      MSG_ROUTING_NONE, NULL, NULL);
  inline_disposition_tab_contents_.reset(new TabContentsWrapper(web_contents));
  inline_disposition_delegate_.reset(
      new WebIntentInlineDispositionDelegate(this, web_contents,
                                             wrapper_->profile()));

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

bool WebIntentPickerGtk::ShouldHaveBorderPadding() const {
  return false;
}

void WebIntentPickerGtk::Observe(int type,
                                 const content::NotificationSource& source,
                                 const content::NotificationDetails& details) {
  DCHECK_EQ(type, chrome::NOTIFICATION_BROWSER_THEME_CHANGED);
  GtkThemeService* theme_service = GetThemeService(wrapper_);
  if (theme_service->UsingNativeTheme())
    gtk_util::UndoForceFontSize(header_label_);
  else
    gtk_util::ForceFontSizePixels(header_label_, kHeaderLabelPixelSize);

  UpdateInstalledServices();
  UpdateSuggestedExtensions();
}

void WebIntentPickerGtk::OnDestroy(GtkWidget* button) {
  // Destroy this object when the contents widget is destroyed. It can't be
  // "delete this" because this function happens in a callback.
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
  model_->set_observer(NULL);
  window_ = NULL;
}

void WebIntentPickerGtk::OnCloseButtonClick(GtkWidget* button) {
  delegate_->OnPickerClosed();
}

void WebIntentPickerGtk::OnExtensionLinkClick(GtkWidget* link) {
  size_t index = GetExtensionWidgetRow(link);
  const WebIntentPickerModel::SuggestedExtension& extension =
      model_->GetSuggestedExtensionAt(index);
  delegate_->OnExtensionLinkClicked(UTF16ToUTF8(extension.id));
}

void WebIntentPickerGtk::OnExtensionInstallButtonClick(GtkWidget* button) {
  size_t index = GetExtensionWidgetRow(button);
  const WebIntentPickerModel::SuggestedExtension& extension =
      model_->GetSuggestedExtensionAt(index);

  delegate_->OnExtensionInstallRequested(UTF16ToUTF8(extension.id));
  SetWidgetsEnabled(false);

  // Re-enable the clicked extension row.
  GList* vbox_list =
      gtk_container_get_children(GTK_CONTAINER(extensions_vbox_));
  GtkWidget* hbox = static_cast<GtkWidget*>(g_list_nth_data(vbox_list, index));
  gtk_widget_set_sensitive(hbox, TRUE);

  // Hide the install button.
  GList* hbox_list = gtk_container_get_children(GTK_CONTAINER(hbox));
  GtkWidget* install_button =
      static_cast<GtkWidget*>(g_list_nth_data(hbox_list, kInstallButtonIndex));
  GtkAllocation allocation;
  gtk_widget_get_allocation(install_button, &allocation);
  gtk_widget_hide(install_button);
  g_list_free(hbox_list);
  g_list_free(vbox_list);

  // Show the throbber with the same size as the install button.
  GtkWidget* throbber = AddThrobberToExtensionAt(index);
  gtk_widget_set_size_request(throbber, allocation.width, allocation.height);
  gtk_widget_show_all(throbber);
}

void WebIntentPickerGtk::OnMoreSuggestionsLinkClick(GtkWidget* link) {
  // TODO(binji): This should link to a CWS search, based on the current
  // action/type pair.
  delegate_->OnSuggestionsLinkClicked();
}

void WebIntentPickerGtk::OnServiceButtonClick(GtkWidget* button) {
  GList* button_list = gtk_container_get_children(GTK_CONTAINER(button_vbox_));
  gint index = g_list_index(button_list, button);
  DCHECK(index != -1);
  g_list_free(button_list);

  const WebIntentPickerModel::InstalledService& installed_service =
      model_->GetInstalledServiceAt(index);

  delegate_->OnServiceChosen(installed_service.url,
                             installed_service.disposition);
}

void WebIntentPickerGtk::InitContents() {
  GtkThemeService* theme_service = GetThemeService(wrapper_);

  // Main contents vbox.
  contents_ = gtk_vbox_new(FALSE, 0);
  gtk_widget_set_size_request(contents_, kMainContentWidth, -1);

  // Hbox containing the close button.
  GtkWidget* close_hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(contents_), close_hbox, TRUE, TRUE, 0);

  close_button_.reset(
      CustomDrawButton::CloseButton(GetThemeService(wrapper_)));
  g_signal_connect(close_button_->widget(), "clicked",
                   G_CALLBACK(OnCloseButtonClickThunk), this);
  gtk_widget_set_can_focus(close_button_->widget(), FALSE);
  gtk_box_pack_end(GTK_BOX(close_hbox), close_button_->widget(),
                   FALSE, FALSE, 0);

  GtkWidget* alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(
      GTK_ALIGNMENT(alignment), 0, ui::kContentAreaBorder,
      ui::kContentAreaBorder, ui::kContentAreaBorder);
  gtk_box_pack_end(GTK_BOX(contents_), alignment, TRUE, TRUE, 0);

  GtkWidget* sub_contents = gtk_vbox_new(FALSE, ui::kContentAreaSpacing);
  gtk_container_add(GTK_CONTAINER(alignment), sub_contents);

  // Hbox containing the header label.
  GtkWidget* header_hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(sub_contents), header_hbox, TRUE, TRUE, 0);

  // Label text will be set in the call to SetActionString().
  header_label_ = theme_service->BuildLabel(std::string(), ui::kGdkBlack);
  gtk_util::ForceFontSizePixels(header_label_, kHeaderLabelPixelSize);
  gtk_box_pack_start(GTK_BOX(header_hbox), header_label_, TRUE, TRUE, 0);
  gtk_misc_set_alignment(GTK_MISC(header_label_), 0, 0);

  // Add separation between the installed services list and the app suggestions.
  GtkWidget* button_alignment = gtk_alignment_new(0.5, 0, 0, 0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(button_alignment), 0,
                            kMainContentPixelSize * 2, 0, 0);

  // Vbox containing all service buttons.
  button_vbox_ = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_container_add(GTK_CONTAINER(button_alignment), button_vbox_);
  gtk_box_pack_start(GTK_BOX(sub_contents), button_alignment, TRUE, TRUE, 0);

  // Chrome Web Store label.
  cws_label_ = theme_service->BuildLabel(
      l10n_util::GetStringUTF8(IDS_INTENT_PICKER_GET_MORE_SERVICES).c_str(),
      ui::kGdkBlack);
  gtk_box_pack_start(GTK_BOX(sub_contents), cws_label_, TRUE, TRUE, 0);
  gtk_misc_set_alignment(GTK_MISC(cws_label_), 0, 0);
  gtk_widget_set_no_show_all(cws_label_, TRUE);

  // Set the label width to the size of |sub_contents|, which we don't have
  // access to yet, by calculating the main content width minus borders.
  gtk_util::SetLabelWidth(cws_label_,
                          kMainContentWidth - 2 * ui::kContentAreaBorder);
  gtk_util::ForceFontSizePixels(cws_label_, kMainContentPixelSize);

  // Suggested extensions vbox.
  extensions_vbox_ = gtk_vbox_new(FALSE, ui::kControlSpacing);
  GtkWidget* indent_extensions = gtk_alignment_new(0.0, 0.5, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(indent_extensions), 0, 0,
                            ui::kGroupIndent, ui::kGroupIndent);
  gtk_container_add(GTK_CONTAINER(indent_extensions), extensions_vbox_);
  gtk_widget_set_no_show_all(indent_extensions, TRUE);
  gtk_box_pack_start(GTK_BOX(sub_contents), indent_extensions, TRUE, TRUE, 0);

  // CWS 'More Suggestions' link.
  GtkWidget* link_alignment = gtk_alignment_new(0, 0.5f, 0, 0);
  GtkWidget* more_suggestions_link = theme_service->BuildChromeLinkButton(
      l10n_util::GetStringUTF8(IDS_INTENT_PICKER_MORE_SUGGESTIONS).c_str());
  gtk_container_add(GTK_CONTAINER(link_alignment), more_suggestions_link);
  gtk_chrome_link_button_set_use_gtk_theme(
      GTK_CHROME_LINK_BUTTON(more_suggestions_link),
      theme_service->UsingNativeTheme());
  gtk_util::ForceFontSizePixels(
      GTK_CHROME_LINK_BUTTON(more_suggestions_link)->label,
      kMainContentPixelSize);
  g_signal_connect(more_suggestions_link, "clicked",
                   G_CALLBACK(OnMoreSuggestionsLinkClickThunk), this);

  GtkWidget* indent_link = gtk_util::IndentWidget(link_alignment);
  gtk_box_pack_start(GTK_BOX(sub_contents), indent_link, TRUE, TRUE, 0);

  // Throbber, which will be added to the hierarchy when necessary.
  throbber_.reset(new ThrobberGtk(theme_service));

  g_signal_connect(contents_, "destroy", G_CALLBACK(&OnDestroyThunk), this);
}

void WebIntentPickerGtk::UpdateInstalledServices() {
  gtk_util::RemoveAllChildren(button_vbox_);

  if (model_->GetInstalledServiceCount() == 0) {
    gtk_widget_hide(gtk_widget_get_parent(button_vbox_));
    return;
  }

  for (size_t i = 0; i < model_->GetInstalledServiceCount(); ++i) {
    const WebIntentPickerModel::InstalledService& installed_service =
        model_->GetInstalledServiceAt(i);

    GtkWidget* button = gtk_button_new();
    gtk_widget_set_tooltip_text(button, installed_service.url.spec().c_str());
    gtk_button_set_label(GTK_BUTTON(button),
                         UTF16ToUTF8(installed_service.title).c_str());
    gtk_button_set_alignment(GTK_BUTTON(button), 0, 0);

    gtk_container_add(GTK_CONTAINER(button_vbox_), button);
    g_signal_connect(button, "clicked", G_CALLBACK(OnServiceButtonClickThunk),
                     this);

    SetServiceButtonImage(button, installed_service.favicon.ToGdkPixbuf());

    // Must be called after SetServiceButtonImage as the internal label widget
    // is replaced in that call.
    SetWidgetFontSize(button, kMainContentPixelSize);
  }

  gtk_widget_show_all(button_vbox_);
  gtk_widget_show(gtk_widget_get_parent(button_vbox_));
}

void WebIntentPickerGtk::UpdateCWSLabel() {
  if (model_->GetInstalledServiceCount() == 0) {
    gtk_widget_hide(gtk_widget_get_parent(button_vbox_));
    gtk_label_set_text(GTK_LABEL(cws_label_), l10n_util::GetStringUTF8(
        IDS_INTENT_PICKER_GET_MORE_SERVICES_NONE_INSTALLED).c_str());
  } else {
    gtk_label_set_text(GTK_LABEL(cws_label_), l10n_util::GetStringUTF8(
        IDS_INTENT_PICKER_GET_MORE_SERVICES).c_str());
    gtk_widget_show(gtk_widget_get_parent(button_vbox_));
  }

  if (model_->GetSuggestedExtensionCount() == 0)
    gtk_widget_hide(cws_label_);
  else
    gtk_widget_show(cws_label_);
}

void WebIntentPickerGtk::UpdateSuggestedExtensions() {
  GtkThemeService* theme_service = GetThemeService(wrapper_);

  gtk_util::RemoveAllChildren(extensions_vbox_);

  if (model_->GetSuggestedExtensionCount() == 0) {
    gtk_widget_hide(gtk_widget_get_parent(extensions_vbox_));
    return;
  }

  gtk_widget_show(gtk_widget_get_parent(extensions_vbox_));

  for (size_t i = 0; i < model_->GetSuggestedExtensionCount(); ++i) {
    const WebIntentPickerModel::SuggestedExtension& extension =
        model_->GetSuggestedExtensionAt(i);

    GtkWidget* hbox = gtk_hbox_new(FALSE, ui::kControlSpacing);
    gtk_box_pack_start(GTK_BOX(extensions_vbox_), hbox, FALSE, FALSE, 0);

    // Icon.
    GtkWidget* icon = gtk_image_new_from_pixbuf(extension.icon.ToGdkPixbuf());
    gtk_box_pack_start(GTK_BOX(hbox), icon, FALSE, FALSE, 0);

    // Title link.
    string16 elided_title = ui::ElideText(extension.title, gfx::Font(),
                                          kTitleLinkMaxWidth, ui::ELIDE_AT_END);
    GtkWidget* title_link = theme_service->BuildChromeLinkButton(
        UTF16ToUTF8(elided_title).c_str());
    gtk_chrome_link_button_set_use_gtk_theme(GTK_CHROME_LINK_BUTTON(title_link),
                                             theme_service->UsingNativeTheme());
    gtk_util::ForceFontSizePixels(GTK_CHROME_LINK_BUTTON(title_link)->label,
                                  kMainContentPixelSize);
    g_signal_connect(title_link, "clicked",
                     G_CALLBACK(OnExtensionLinkClickThunk), this);
    gtk_box_pack_start(GTK_BOX(hbox), title_link, FALSE, FALSE, 0);

    // Stars.
    GtkWidget* stars = CreateStarsWidget(extension.average_rating);
    gtk_box_pack_start(GTK_BOX(hbox), stars, FALSE, FALSE, 0);

    // Install button.
    GtkWidget* install_button = gtk_button_new();
    gtk_button_set_label(
        GTK_BUTTON(install_button),
        l10n_util::GetStringUTF8(IDS_INTENT_PICKER_INSTALL_EXTENSION).c_str());
    GtkWidget* label = gtk_bin_get_child(GTK_BIN(install_button));
    gtk_util::ForceFontSizePixels(label, kMainContentPixelSize);
    g_signal_connect(install_button, "clicked",
                     G_CALLBACK(OnExtensionInstallButtonClickThunk), this);
    gtk_box_pack_end(GTK_BOX(hbox), install_button, FALSE, FALSE, 0);
  }

  gtk_widget_show_all(extensions_vbox_);
  gtk_widget_show(gtk_widget_get_parent(extensions_vbox_));
}

void WebIntentPickerGtk::SetWidgetsEnabled(bool enabled) {
  gboolean data = enabled;
  gtk_container_foreach(GTK_CONTAINER(button_vbox_), EnableWidgetCallback,
                        &data);
  gtk_container_foreach(GTK_CONTAINER(extensions_vbox_), EnableWidgetCallback,
                        &data);
}

GtkWidget* WebIntentPickerGtk::AddThrobberToExtensionAt(size_t index) {
  // The throbber should be unparented.
  DCHECK(!gtk_widget_get_parent(throbber_->widget()));
  GList* vbox_list =
      gtk_container_get_children(GTK_CONTAINER(extensions_vbox_));
  GtkWidget* hbox = static_cast<GtkWidget*>(g_list_nth_data(vbox_list, index));
  GtkWidget* alignment = gtk_alignment_new(0.5, 0.5, 0, 0);
  gtk_container_add(GTK_CONTAINER(alignment), throbber_->widget());
  gtk_box_pack_end(GTK_BOX(hbox), alignment, FALSE, FALSE, 0);
  g_list_free(vbox_list);
  throbber_->Start();
  return alignment;
}

void WebIntentPickerGtk::RemoveThrobber() {
  GtkWidget* alignment = gtk_widget_get_parent(throbber_->widget());
  DCHECK(alignment);
  gtk_container_remove(GTK_CONTAINER(alignment), throbber_->widget());
  gtk_widget_destroy(alignment);
  throbber_->Stop();
}
