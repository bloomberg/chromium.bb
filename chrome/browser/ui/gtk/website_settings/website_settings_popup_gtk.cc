// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/website_settings/website_settings_popup_gtk.h"

#include "base/i18n/rtl.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/collected_cookies_gtk.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/location_bar_view_gtk.h"
#include "chrome/browser/ui/gtk/website_settings/permission_selector.h"
#include "chrome/browser/ui/tab_contents/tab_contents.h"
#include "chrome/browser/ui/website_settings/website_settings.h"
#include "chrome/browser/ui/website_settings/website_settings_utils.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/user_metrics.h"
#include "googleurl/src/gurl.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "grit/ui_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::OpenURLParams;

namespace {

// The width of the popup.
const int kPopupWidth = 400;

// The max width of the text labels on the connection tab.
const int kConnectionTabTextWidth = 300;

// The padding used for sections on the connection tab.
const int kConnectionTabSectionPadding = 10;

// The background color of the tabs if a theme other than the native GTK theme
// is selected.
const GdkColor kBackgroundColor = GDK_COLOR_RGB(0xff, 0xff, 0xff);
// The text color of the site identity status label for websites with a
// verified identity.
const GdkColor kGdkGreen = GDK_COLOR_RGB(0x29, 0x8a, 0x27);

GtkWidget* CreateTextLabel(const std::string& text,
                           int width,
                           GtkThemeService* theme_service) {
  GtkWidget* label = theme_service->BuildLabel(text, ui::kGdkBlack);
  if (width > 0)
    gtk_util::SetLabelWidth(label, width);
  gtk_label_set_selectable(GTK_LABEL(label), TRUE);
  gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
  return label;
}

void ClearContainer(GtkWidget* container) {
  GList* child = gtk_container_get_children(GTK_CONTAINER(container));
  while (child) {
    gtk_container_remove(GTK_CONTAINER(container), GTK_WIDGET(child->data));
    child = child->next;
  }
}

void SetConnectionSection(GtkWidget* section_box,
                          const gfx::Image& icon,
                          GtkWidget* content_box) {
  DCHECK(section_box);
  ClearContainer(section_box);
  gtk_container_set_border_width(GTK_CONTAINER(section_box),
                                 kConnectionTabSectionPadding);

  GtkWidget* hbox = gtk_hbox_new(FALSE, ui::kControlSpacing);

  GdkPixbuf* pixbuf = icon.ToGdkPixbuf();
  GtkWidget* image = gtk_image_new_from_pixbuf(pixbuf);
  gtk_box_pack_start(GTK_BOX(hbox), image, FALSE, FALSE, 0);
  gtk_misc_set_alignment(GTK_MISC(image), 0, 0);

  gtk_box_pack_start(GTK_BOX(hbox), content_box, TRUE, TRUE, 0);

  gtk_box_pack_start(GTK_BOX(section_box), hbox, TRUE, TRUE, 0);
  gtk_widget_show_all(section_box);
}

GtkWidget* CreatePermissionTabSection(std::string section_title,
                                      GtkWidget* section_content,
                                      GtkThemeService* theme_service) {
  GtkWidget* section_box = gtk_vbox_new(FALSE, ui::kControlSpacing);

  // Add Section title
  GtkWidget* title_hbox = gtk_hbox_new(FALSE, ui::kControlSpacing);

  GtkWidget* label = theme_service->BuildLabel(section_title, ui::kGdkBlack);
  gtk_label_set_selectable(GTK_LABEL(label), TRUE);
  PangoAttrList* attributes = pango_attr_list_new();
  pango_attr_list_insert(attributes,
                         pango_attr_weight_new(PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes(GTK_LABEL(label), attributes);
  pango_attr_list_unref(attributes);
  gtk_box_pack_start(GTK_BOX(section_box), title_hbox, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(title_hbox), label, FALSE, FALSE, 0);

  // Add section content
  gtk_box_pack_start(GTK_BOX(section_box), section_content, FALSE, FALSE, 0);
  return section_box;
}


class InternalPageInfoPopupGtk : public BubbleDelegateGtk {
 public:
  explicit InternalPageInfoPopupGtk(gfx::NativeWindow parent,
                                          Profile* profile);
  virtual ~InternalPageInfoPopupGtk();

 private:
  // BubbleDelegateGtk implementation.
  virtual void BubbleClosing(BubbleGtk* bubble, bool closed_by_escape) OVERRIDE;

  // The popup bubble container.
  BubbleGtk* bubble_;

  DISALLOW_COPY_AND_ASSIGN(InternalPageInfoPopupGtk);
};

InternalPageInfoPopupGtk::InternalPageInfoPopupGtk(
    gfx::NativeWindow parent, Profile* profile) {
  GtkWidget* contents = gtk_hbox_new(FALSE, ui::kContentAreaSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(contents),
                                 ui::kContentAreaBorder);
  // Add the popup icon.
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  GdkPixbuf* pixbuf = rb.GetNativeImageNamed(IDR_PRODUCT_LOGO_26).ToGdkPixbuf();
  GtkWidget* image = gtk_image_new_from_pixbuf(pixbuf);
  gtk_box_pack_start(GTK_BOX(contents), image, FALSE, FALSE, 0);
  gtk_misc_set_alignment(GTK_MISC(image), 0, 0);

  // Add the popup text.
  GtkThemeService* theme_service = GtkThemeService::GetFrom(profile);
  GtkWidget* label = theme_service->BuildLabel(
      l10n_util::GetStringUTF8(IDS_PAGE_INFO_INTERNAL_PAGE), ui::kGdkBlack);
  gtk_label_set_selectable(GTK_LABEL(label), FALSE);
  PangoAttrList* attributes = pango_attr_list_new();
  pango_attr_list_insert(attributes,
                         pango_attr_weight_new(PANGO_WEIGHT_BOLD));
  gtk_box_pack_start(GTK_BOX(contents), label, FALSE, FALSE, 0);

  gtk_widget_show_all(contents);

  // Create the bubble.
  BubbleGtk::ArrowLocationGtk arrow_location = base::i18n::IsRTL() ?
      BubbleGtk::ARROW_LOCATION_TOP_RIGHT :
      BubbleGtk::ARROW_LOCATION_TOP_LEFT;
  BrowserWindowGtk* browser_window =
      BrowserWindowGtk::GetBrowserWindowForNativeWindow(parent);
  GtkWidget* anchor = browser_window->
      GetToolbar()->GetLocationBarView()->location_icon_widget();
  bubble_ = BubbleGtk::Show(anchor,
                            NULL,  // |rect|
                            contents,
                            arrow_location,
                            BubbleGtk::MATCH_SYSTEM_THEME |
                                BubbleGtk::POPUP_WINDOW |
                                BubbleGtk::GRAB_INPUT,
                            theme_service,
                            this);  // |delegate|
  DCHECK(bubble_);
}

InternalPageInfoPopupGtk::~InternalPageInfoPopupGtk() {
}

void InternalPageInfoPopupGtk::BubbleClosing(BubbleGtk* bubble,
                                             bool closed_by_escape) {
  delete this;
}

}  // namespace

// static
void WebsiteSettingsPopupGtk::Show(gfx::NativeWindow parent,
                                   Profile* profile,
                                   TabContents* tab_contents,
                                   const GURL& url,
                                   const content::SSLStatus& ssl) {
  if (InternalChromePage(url))
    new InternalPageInfoPopupGtk(parent, profile);
  else
    new WebsiteSettingsPopupGtk(parent, profile, tab_contents, url, ssl);
}

WebsiteSettingsPopupGtk::WebsiteSettingsPopupGtk(
    gfx::NativeWindow parent,
    Profile* profile,
    TabContents* tab_contents,
    const GURL& url,
    const content::SSLStatus& ssl)
    : parent_(parent),
      contents_(NULL),
      theme_service_(GtkThemeService::GetFrom(profile)),
      profile_(profile),
      tab_contents_(tab_contents),
      browser_(NULL),
      cert_id_(0),
      header_box_(NULL),
      cookies_section_contents_(NULL),
      permissions_section_contents_(NULL),
      identity_contents_(NULL),
      connection_contents_(NULL),
      first_visit_contents_(NULL),
      notebook_(NULL),
      presenter_(NULL) {
  BrowserWindowGtk* browser_window =
      BrowserWindowGtk::GetBrowserWindowForNativeWindow(parent);
  browser_ = browser_window->browser();
  anchor_ = browser_window->
      GetToolbar()->GetLocationBarView()->location_icon_widget();

  InitContents();

  BubbleGtk::ArrowLocationGtk arrow_location = base::i18n::IsRTL() ?
      BubbleGtk::ARROW_LOCATION_TOP_RIGHT :
      BubbleGtk::ARROW_LOCATION_TOP_LEFT;
  bubble_ = BubbleGtk::Show(anchor_,
                            NULL,  // |rect|
                            contents_,
                            arrow_location,
                            BubbleGtk::MATCH_SYSTEM_THEME |
                                BubbleGtk::POPUP_WINDOW |
                                BubbleGtk::GRAB_INPUT,
                            theme_service_,
                            this);  // |delegate|
  if (!bubble_) {
    NOTREACHED();
    return;
  }

  presenter_.reset(new WebsiteSettings(this, profile,
                                       tab_contents->content_settings(),
                                       tab_contents->infobar_tab_helper(),
                                       url, ssl,
                                       content::CertStore::GetInstance()));
}

WebsiteSettingsPopupGtk::~WebsiteSettingsPopupGtk() {
}

void WebsiteSettingsPopupGtk::BubbleClosing(BubbleGtk* bubble,
                                            bool closed_by_escape) {
  if (presenter_.get()) {
    presenter_->OnUIClosing();
    presenter_.reset();
  }
  delete this;
}

void WebsiteSettingsPopupGtk::InitContents() {
  if (!contents_) {
    contents_ = gtk_vbox_new(FALSE, ui::kContentAreaSpacing);
    gtk_container_set_border_width(GTK_CONTAINER(contents_),
                                   ui::kContentAreaBorder);
  } else {
    gtk_util::RemoveAllChildren(contents_);
  }

  // Create popup header.
  header_box_ = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(contents_), header_box_, FALSE, FALSE, 0);

  // Create the container for the contents of the permissions tab.
  GtkWidget* permission_tab_contents = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(permission_tab_contents), 10);
  cookies_section_contents_ = gtk_vbox_new(FALSE, ui::kControlSpacing);
  std::string title = l10n_util::GetStringUTF8(
      IDS_WEBSITE_SETTINGS_TITLE_SITE_DATA);
  gtk_box_pack_start(GTK_BOX(permission_tab_contents),
                     CreatePermissionTabSection(title,
                                                cookies_section_contents_,
                                                theme_service_),
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(permission_tab_contents),
                     gtk_hseparator_new(),
                     FALSE, FALSE, 0);
  permissions_section_contents_ = gtk_vbox_new(FALSE, ui::kControlSpacing);
  title = l10n_util::GetStringUTF8(IDS_WEBSITE_SETTINGS_TITLE_SITE_PERMISSIONS);
  gtk_box_pack_start(GTK_BOX(permission_tab_contents),
                     CreatePermissionTabSection(title,
                                                permissions_section_contents_,
                                                theme_service_),
                     FALSE, FALSE, 0);

  // Create the container for the contents of the identity tab.
  GtkWidget* connection_tab = gtk_vbox_new(FALSE, ui::kControlSpacing);
  identity_contents_ = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(connection_tab), identity_contents_, FALSE, FALSE,
                     0);
  gtk_box_pack_start(GTK_BOX(connection_tab), gtk_hseparator_new(), FALSE,
                     FALSE, 0);
  connection_contents_ = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(connection_tab), connection_contents_, FALSE,
                     FALSE, 0);
  gtk_box_pack_start(GTK_BOX(connection_tab), gtk_hseparator_new(), FALSE,
                     FALSE, 0);
  first_visit_contents_ = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(connection_tab), first_visit_contents_, FALSE,
                     FALSE, 0);
  gtk_box_pack_start(GTK_BOX(connection_tab), gtk_hseparator_new(), FALSE,
                     FALSE, 0);

  GtkWidget* help_link = theme_service_->BuildChromeLinkButton(
      l10n_util::GetStringUTF8(IDS_PAGE_INFO_HELP_CENTER_LINK));
  GtkWidget* help_link_hbox = gtk_hbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(help_link_hbox),
                                 kConnectionTabSectionPadding);
  gtk_box_pack_start(GTK_BOX(help_link_hbox), help_link, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(connection_tab), help_link_hbox, FALSE, FALSE, 0);
  g_signal_connect(help_link, "clicked",
                   G_CALLBACK(OnHelpLinkClickedThunk), this);

  // Create tab container and add all tabs.
  notebook_ = gtk_notebook_new();
  if (theme_service_->UsingNativeTheme())
    gtk_widget_modify_bg(notebook_, GTK_STATE_NORMAL, NULL);
  else
    gtk_widget_modify_bg(notebook_, GTK_STATE_NORMAL, &kBackgroundColor);

  GtkWidget* label = theme_service_->BuildLabel(
      l10n_util::GetStringUTF8(IDS_WEBSITE_SETTINGS_TAB_LABEL_PERMISSIONS),
      ui::kGdkBlack);
  gtk_widget_show(label);
  gtk_notebook_insert_page(GTK_NOTEBOOK(notebook_), permission_tab_contents,
                           label, TAB_ID_PERMISSIONS);

  label = theme_service_->BuildLabel(
      l10n_util::GetStringUTF8(IDS_WEBSITE_SETTINGS_TAB_LABEL_CONNECTION),
      ui::kGdkBlack);
  gtk_widget_show(label);
  gtk_notebook_insert_page(GTK_NOTEBOOK(notebook_), connection_tab, label,
                           TAB_ID_CONNECTION);

  DCHECK_EQ(gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook_)), NUM_TAB_IDS);

  gtk_box_pack_start(GTK_BOX(contents_), notebook_, FALSE, FALSE, 0);
  gtk_widget_show_all(contents_);
}

void WebsiteSettingsPopupGtk::OnPermissionChanged(
    PermissionSelector* selector) {
  presenter_->OnSitePermissionChanged(selector->GetType(),
                                      selector->GetSetting());
}

void WebsiteSettingsPopupGtk::OnComboboxShown() {
  bubble_->HandlePointerAndKeyboardUngrabbedByContent();
}

void WebsiteSettingsPopupGtk::SetCookieInfo(
    const CookieInfoList& cookie_info_list) {
  DCHECK(cookies_section_contents_);
  ClearContainer(cookies_section_contents_);

  // Create cookies info rows.
  for (CookieInfoList::const_iterator it = cookie_info_list.begin();
       it != cookie_info_list.end();
       ++it) {
    // Create the cookies info box.
    GtkWidget* cookies_info = gtk_hbox_new(FALSE, 0);

    // Add the icon to the cookies info box
    GdkPixbuf* pixbuf = WebsiteSettingsUI::GetPermissionIcon(
        CONTENT_SETTINGS_TYPE_COOKIES, CONTENT_SETTING_ALLOW).ToGdkPixbuf();
    GtkWidget* image = gtk_image_new_from_pixbuf(pixbuf);
    gtk_misc_set_alignment(GTK_MISC(image), 0, 0);
    gtk_box_pack_start(GTK_BOX(cookies_info), image, FALSE, FALSE, 0);

    // Add the allowed and blocked cookies counts to the cookies info box.
    std::string info_str = l10n_util::GetStringFUTF8(
        IDS_WEBSITE_SETTINGS_SITE_DATA_STATS_LINE,
        UTF8ToUTF16(it->cookie_source),
        base::IntToString16(it->allowed),
        base::IntToString16(it->blocked));

    GtkWidget* info = theme_service_->BuildLabel(info_str, ui::kGdkBlack);
    gtk_label_set_selectable(GTK_LABEL(info), TRUE);
    const int kPadding = 4;
    gtk_box_pack_start(GTK_BOX(cookies_info), info, FALSE, FALSE, kPadding);

    // Add the cookies info box to the section box.
    gtk_box_pack_start(GTK_BOX(cookies_section_contents_),
                       cookies_info,
                       FALSE, FALSE, 0);
  }

  // Create row with links for cookie settings and for the cookies dialog.
  GtkWidget* link_hbox = gtk_hbox_new(FALSE, 0);

  GtkWidget* view_cookies_link = theme_service_->BuildChromeLinkButton(
      l10n_util::GetStringUTF8(IDS_WEBSITE_SETTINGS_SHOW_SITE_DATA));
  g_signal_connect(view_cookies_link, "clicked",
                   G_CALLBACK(OnCookiesLinkClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(link_hbox), view_cookies_link,
                     FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(cookies_section_contents_), link_hbox,
                     TRUE, FALSE, 0);

  gtk_widget_show_all(cookies_section_contents_);
}

void WebsiteSettingsPopupGtk::SetIdentityInfo(
    const IdentityInfo& identity_info) {
  // Create popup header.
  DCHECK(header_box_);
  ClearContainer(header_box_);

  GtkWidget* hbox = gtk_hbox_new(FALSE, 0);
  GtkWidget* identity_label = theme_service_->BuildLabel(
      identity_info.site_identity, ui::kGdkBlack);
  gtk_label_set_selectable(GTK_LABEL(identity_label), TRUE);
  PangoAttrList* attributes = pango_attr_list_new();
  pango_attr_list_insert(attributes,
                         pango_attr_weight_new(PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes(GTK_LABEL(identity_label), attributes);
  pango_attr_list_unref(attributes);
  gtk_box_pack_start(GTK_BOX(hbox), identity_label, FALSE, FALSE, 0);
  GtkWidget* close_button = gtk_button_new();
  gtk_button_set_relief(GTK_BUTTON(close_button), GTK_RELIEF_NONE);
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  GdkPixbuf* pixbuf = rb.GetNativeImageNamed(IDR_CLOSE_BAR_H).ToGdkPixbuf();
  GtkWidget* image = gtk_image_new_from_pixbuf(pixbuf);
  gtk_button_set_image(GTK_BUTTON(close_button), image);
  g_signal_connect(close_button, "clicked",
                   G_CALLBACK(OnCloseButtonClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(hbox), close_button, FALSE, FALSE, 0);
  int label_width = kPopupWidth - gdk_pixbuf_get_width(pixbuf);
  gtk_util::SetLabelWidth(identity_label, label_width);
  gtk_box_pack_start(GTK_BOX(header_box_), hbox, FALSE, FALSE, 0);

  std::string identity_status_text;
  const GdkColor* color =
      theme_service_->UsingNativeTheme() ? NULL : &ui::kGdkBlack;

  switch (identity_info.identity_status) {
    case WebsiteSettings::SITE_IDENTITY_STATUS_CERT:
    case WebsiteSettings::SITE_IDENTITY_STATUS_DNSSEC_CERT:
    case WebsiteSettings::SITE_IDENTITY_STATUS_EV_CERT:
      identity_status_text =
          l10n_util::GetStringUTF8(IDS_WEBSITE_SETTINGS_IDENTITY_VERIFIED);
      if (!theme_service_->UsingNativeTheme())
        color = &kGdkGreen;
      break;
    default:
      identity_status_text =
          l10n_util::GetStringUTF8(IDS_WEBSITE_SETTINGS_IDENTITY_NOT_VERIFIED);
      break;
  }
  GtkWidget* status_label =
      CreateTextLabel(identity_status_text, kPopupWidth, theme_service_);
  gtk_widget_modify_fg(status_label, GTK_STATE_NORMAL, color);
  gtk_box_pack_start(
      GTK_BOX(header_box_), status_label, FALSE, FALSE, 0);
  gtk_widget_show_all(header_box_);

  // Create identity section.
  GtkWidget* section_content = gtk_vbox_new(FALSE, ui::kControlSpacing);
  GtkWidget* identity_description =
      CreateTextLabel(identity_info.identity_status_description,
                      kConnectionTabTextWidth, theme_service_);
  gtk_box_pack_start(GTK_BOX(section_content), identity_description, FALSE,
                     FALSE, 0);
  if (identity_info.cert_id) {
    cert_id_ = identity_info.cert_id;
    GtkWidget* view_cert_link = theme_service_->BuildChromeLinkButton(
        l10n_util::GetStringUTF8(IDS_PAGEINFO_CERT_INFO_BUTTON));
    g_signal_connect(view_cert_link, "clicked",
                     G_CALLBACK(OnViewCertLinkClickedThunk), this);
    GtkWidget* link_hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(link_hbox), view_cert_link,
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(section_content), link_hbox, FALSE, FALSE, 0);
  }
  SetConnectionSection(
      identity_contents_,
      WebsiteSettingsUI::GetIdentityIcon(identity_info.identity_status),
      section_content);

  // Create connection section.
  GtkWidget* connection_description =
      CreateTextLabel(identity_info.connection_status_description,
                      kConnectionTabTextWidth, theme_service_);
  section_content = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(section_content), connection_description, FALSE,
                     FALSE, 0);
  SetConnectionSection(
      connection_contents_,
      WebsiteSettingsUI::GetConnectionIcon(identity_info.connection_status),
      section_content);
}

void WebsiteSettingsPopupGtk::SetFirstVisit(const string16& first_visit) {
  GtkWidget* titel = theme_service_->BuildLabel(
      l10n_util::GetStringUTF8(IDS_PAGE_INFO_SITE_INFO_TITLE),
      ui::kGdkBlack);
  gtk_label_set_selectable(GTK_LABEL(titel), TRUE);
  PangoAttrList* attributes = pango_attr_list_new();
  pango_attr_list_insert(attributes,
                         pango_attr_weight_new(PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes(GTK_LABEL(titel), attributes);
  pango_attr_list_unref(attributes);
  gtk_misc_set_alignment(GTK_MISC(titel), 0, 0);

  GtkWidget* first_visit_label = CreateTextLabel(UTF16ToUTF8(first_visit),
                                                 kConnectionTabTextWidth,
                                                 theme_service_);
  GtkWidget* section_contents = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(section_contents), titel, FALSE, FALSE, 0);
  gtk_box_pack_start(
      GTK_BOX(section_contents), first_visit_label, FALSE, FALSE, 0);

  SetConnectionSection(
      first_visit_contents_,
      WebsiteSettingsUI::GetFirstVisitIcon(first_visit),
      section_contents);
}

void WebsiteSettingsPopupGtk::SetPermissionInfo(
      const PermissionInfoList& permission_info_list) {
  DCHECK(permissions_section_contents_);
  ClearContainer(permissions_section_contents_);
  // Clear the map since the UI is reconstructed.
  selectors_.clear();

  for (PermissionInfoList::const_iterator permission =
           permission_info_list.begin();
       permission != permission_info_list.end();
       ++permission) {
    PermissionSelector* selector =
        new PermissionSelector(
           theme_service_,
           permission->type,
           permission->setting,
           permission->default_setting,
           permission->source);
    selector->AddObserver(this);
    GtkWidget* hbox = selector->widget();
    selectors_.push_back(selector);
    gtk_box_pack_start(GTK_BOX(permissions_section_contents_), hbox, FALSE,
                       FALSE, 0);
  }

  gtk_widget_show_all(permissions_section_contents_);
}

void WebsiteSettingsPopupGtk::SetSelectedTab(TabId tab_id) {
  DCHECK(notebook_);
  gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_),
                                static_cast<gint>(tab_id));
}

void WebsiteSettingsPopupGtk::OnCookiesLinkClicked(GtkWidget* widget) {
  // Count how often the Collected Cookies dialog is opened.
  content::RecordAction(
      content::UserMetricsAction("WebsiteSettings_CookiesDialogOpened"));

  new CollectedCookiesGtk(GTK_WINDOW(parent_),
                          tab_contents_);
  bubble_->Close();
}

void WebsiteSettingsPopupGtk::OnViewCertLinkClicked(GtkWidget* widget) {
  DCHECK_NE(cert_id_, 0);
  ShowCertificateViewerByID(
      tab_contents_->web_contents(), GTK_WINDOW(parent_), cert_id_);
  bubble_->Close();
}

void WebsiteSettingsPopupGtk::OnCloseButtonClicked(GtkWidget* widget) {
  bubble_->Close();
}

void WebsiteSettingsPopupGtk::OnHelpLinkClicked(GtkWidget* widget) {
  browser_->OpenURL(OpenURLParams(GURL(chrome::kPageInfoHelpCenterURL),
                    content::Referrer(),
                    NEW_FOREGROUND_TAB,
                    content::PAGE_TRANSITION_LINK,
                     false));
  bubble_->Close();
}
