// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/website_settings/website_settings_popup_gtk.h"

#include "base/i18n/rtl.h"
#include "base/strings/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/collected_cookies_gtk.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/location_bar_view_gtk.h"
#include "chrome/browser/ui/gtk/nine_box.h"
#include "chrome/browser/ui/gtk/website_settings/permission_selector.h"
#include "chrome/browser/ui/website_settings/website_settings.h"
#include "chrome/browser/ui/website_settings/website_settings_utils.h"
#include "chrome/common/chrome_notification_types.h"
#include "chrome/common/url_constants.h"
#include "content/public/browser/cert_store.h"
#include "content/public/browser/notification_service.h"
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
#include "ui/gfx/image/cairo_cached_surface.h"
#include "ui/gfx/image/image.h"

using content::OpenURLParams;

namespace {

// The width of the popup.
const int kPopupWidth = 400;

// Spacing between consecutive tabs in the tabstrip.
const int kInterTabSpacing = 2;

// Spacing between start of tab and tab text.
const int kTabTextHorizontalMargin = 12;

// The vertical lift of the tab's text off the bottom of the tab.
const int kTabTextBaseline = 4;

// The max width of the text labels on the connection tab.
const int kConnectionTabTextWidth = 300;

// The text color of the site identity status label for websites with a
// verified identity.
const GdkColor kGdkGreen = GDK_COLOR_RGB(0x29, 0x8a, 0x27);

GtkWidget* CreateTextLabel(const std::string& text,
                           int width,
                           GtkThemeService* theme_service,
                           const GdkColor& color) {
  GtkWidget* label = theme_service->BuildLabel(text, color);
  if (width > 0)
    gtk_util::SetLabelWidth(label, width);
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
                                 ui::kContentAreaBorder);

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

// A popup that is shown for internal pages (like chrome://).
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
  GtkWidget* contents = gtk_hbox_new(FALSE, ui::kLabelSpacing);
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
  PangoAttrList* attributes = pango_attr_list_new();
  pango_attr_list_insert(attributes,
                         pango_attr_weight_new(PANGO_WEIGHT_BOLD));
  gtk_box_pack_start(GTK_BOX(contents), label, FALSE, FALSE, 0);

  gtk_widget_show_all(contents);

  // Create the bubble.
  BrowserWindowGtk* browser_window =
      BrowserWindowGtk::GetBrowserWindowForNativeWindow(parent);
  GtkWidget* anchor = browser_window->
      GetToolbar()->GetLocationBarView()->location_icon_widget();
  bubble_ = BubbleGtk::Show(anchor,
                            NULL,  // |rect|
                            contents,
                            BubbleGtk::ANCHOR_TOP_LEFT,
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
                                   content::WebContents* web_contents,
                                   const GURL& url,
                                   const content::SSLStatus& ssl) {
  if (InternalChromePage(url))
    new InternalPageInfoPopupGtk(parent, profile);
  else
    new WebsiteSettingsPopupGtk(parent, profile, web_contents, url, ssl);
}

WebsiteSettingsPopupGtk::WebsiteSettingsPopupGtk(
    gfx::NativeWindow parent,
    Profile* profile,
    content::WebContents* web_contents,
    const GURL& url,
    const content::SSLStatus& ssl)
    : parent_(parent),
      contents_(NULL),
      theme_service_(GtkThemeService::GetFrom(profile)),
      profile_(profile),
      web_contents_(web_contents),
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

  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(theme_service_));

  InitContents();

  bubble_ = BubbleGtk::Show(anchor_,
                            NULL,  // |rect|
                            contents_,
                            BubbleGtk::ANCHOR_TOP_LEFT,
                            BubbleGtk::MATCH_SYSTEM_THEME |
                                BubbleGtk::POPUP_WINDOW |
                                BubbleGtk::GRAB_INPUT,
                            theme_service_,
                            this);  // |delegate|
  if (!bubble_) {
    NOTREACHED();
    return;
  }

  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents);
  InfoBarService* infobar_service =
      InfoBarService::FromWebContents(web_contents);
  presenter_.reset(new WebsiteSettings(this, profile,
                                       content_settings,
                                       infobar_service,
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

  // Slightly delay destruction to allow the event stack to unwind and release
  // references to owned widgets.
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void WebsiteSettingsPopupGtk::InitContents() {
  if (!contents_)
    contents_ = gtk_vbox_new(FALSE, 0);
  else
    gtk_util::RemoveAllChildren(contents_);

  // Create popup header.
  header_box_ = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(contents_), header_box_, FALSE, FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(header_box_),
                                 ui::kContentAreaBorder);

  // Create the container for the contents of the permissions tab.
  GtkWidget* permission_tab_contents =
      gtk_vbox_new(FALSE, ui::kContentAreaSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(permission_tab_contents),
                                 ui::kContentAreaBorder);
  cookies_section_contents_ = gtk_vbox_new(FALSE, ui::kControlSpacing);
  std::string title = l10n_util::GetStringUTF8(
      IDS_WEBSITE_SETTINGS_TITLE_SITE_DATA);
  gtk_box_pack_start(GTK_BOX(permission_tab_contents),
                     CreatePermissionTabSection(title,
                                                cookies_section_contents_,
                                                theme_service_),
                     FALSE, FALSE, 0);
  permissions_section_contents_ = gtk_vbox_new(FALSE, ui::kControlSpacing);
  title = l10n_util::GetStringUTF8(IDS_WEBSITE_SETTINGS_TITLE_SITE_PERMISSIONS);
  gtk_box_pack_start(GTK_BOX(permission_tab_contents),
                     CreatePermissionTabSection(title,
                                                permissions_section_contents_,
                                                theme_service_),
                     FALSE, FALSE, 0);

  // Create the container for the contents of the connection tab.
  GtkWidget* connection_tab = gtk_vbox_new(FALSE, 0);
  identity_contents_ = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(connection_tab), identity_contents_, FALSE, FALSE,
                     0);
  gtk_box_pack_start(GTK_BOX(connection_tab), gtk_hseparator_new(), FALSE,
                     FALSE, 0);
  connection_contents_ = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(connection_tab), connection_contents_, FALSE,
                     FALSE, 0);
  gtk_box_pack_start(GTK_BOX(connection_tab), gtk_hseparator_new(), FALSE,
                     FALSE, 0);
  first_visit_contents_ = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(connection_tab), first_visit_contents_, FALSE,
                     FALSE, 0);
  gtk_box_pack_start(GTK_BOX(connection_tab), gtk_hseparator_new(), FALSE,
                     FALSE, 0);

  GtkWidget* help_link = theme_service_->BuildChromeLinkButton(
      l10n_util::GetStringUTF8(IDS_PAGE_INFO_HELP_CENTER_LINK));
  GtkWidget* help_link_hbox = gtk_hbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(help_link_hbox),
                                 ui::kContentAreaBorder);
  gtk_box_pack_start(GTK_BOX(help_link_hbox), help_link, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(connection_tab), help_link_hbox, FALSE, FALSE, 0);
  g_signal_connect(help_link, "clicked",
                   G_CALLBACK(OnHelpLinkClickedThunk), this);

  // Create tabstrip (used only for Chrome-theme mode).
  GtkWidget* tabstrip = gtk_hbox_new(FALSE, kInterTabSpacing);
  tabstrip_alignment_ = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(tabstrip_alignment_), 0, 0,
                            ui::kContentAreaBorder, ui::kContentAreaBorder);
  int tab_height = ui::ResourceBundle::GetSharedInstance().GetNativeImageNamed(
      IDR_WEBSITE_SETTINGS_TAB_LEFT2).ToImageSkia()->height();
  gtk_widget_set_size_request(tabstrip_alignment_, -1, tab_height);
  g_signal_connect(tabstrip_alignment_, "expose-event",
                   G_CALLBACK(&OnTabstripExposeThunk), this);
  gtk_container_add(GTK_CONTAINER(tabstrip_alignment_), tabstrip);
  gtk_box_pack_start(GTK_BOX(contents_), tabstrip_alignment_, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(tabstrip),
      BuildTab(IDS_WEBSITE_SETTINGS_TAB_LABEL_PERMISSIONS),
      FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(tabstrip),
      BuildTab(IDS_WEBSITE_SETTINGS_TAB_LABEL_CONNECTION),
      FALSE, FALSE, 0);

  // Create tab container and add all tabs.
  notebook_ = gtk_notebook_new();
  gtk_notebook_set_tab_hborder(GTK_NOTEBOOK(notebook_), 0);

  GtkWidget* label = gtk_label_new(l10n_util::GetStringUTF8(
      IDS_WEBSITE_SETTINGS_TAB_LABEL_PERMISSIONS).c_str());
  gtk_misc_set_padding(GTK_MISC(label), kTabTextHorizontalMargin, 0);
  gtk_notebook_insert_page(GTK_NOTEBOOK(notebook_), permission_tab_contents,
                           label, TAB_ID_PERMISSIONS);

  label = gtk_label_new(l10n_util::GetStringUTF8(
      IDS_WEBSITE_SETTINGS_TAB_LABEL_CONNECTION).c_str());
  gtk_misc_set_padding(GTK_MISC(label), kTabTextHorizontalMargin, 0);
  gtk_notebook_insert_page(GTK_NOTEBOOK(notebook_), connection_tab, label,
                           TAB_ID_CONNECTION);

  DCHECK_EQ(gtk_notebook_get_n_pages(GTK_NOTEBOOK(notebook_)), NUM_TAB_IDS);

  gtk_box_pack_start(GTK_BOX(contents_), notebook_, FALSE, FALSE, 0);

  theme_service_->InitThemesFor(this);
  gtk_widget_show_all(contents_);
}

GtkWidget* WebsiteSettingsPopupGtk::BuildTab(int ids) {
  GtkWidget* tab = gtk_event_box_new();
  gtk_event_box_set_visible_window(GTK_EVENT_BOX(tab), FALSE);
  GtkWidget* label = gtk_label_new(l10n_util::GetStringUTF8(ids).c_str());
  gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &ui::kGdkBlack);
  gtk_misc_set_padding(GTK_MISC(label),
                       kTabTextHorizontalMargin, kTabTextBaseline);
  gtk_misc_set_alignment(GTK_MISC(label), 0.5, 1.0);
  gtk_container_add(GTK_CONTAINER(tab), label);
  g_signal_connect(tab, "button-press-event",
                   G_CALLBACK(&OnTabButtonPressThunk), this);
  g_signal_connect(tab, "expose-event",
                   G_CALLBACK(&OnTabExposeThunk), this);
  return tab;
}

void WebsiteSettingsPopupGtk::Observe(
    int type,
    const content::NotificationSource& source,
    const content::NotificationDetails& details) {
  DCHECK_EQ(chrome::NOTIFICATION_BROWSER_THEME_CHANGED, type);

  if (theme_service_->UsingNativeTheme()) {
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook_), TRUE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(notebook_), TRUE);
    gtk_widget_set_no_show_all(tabstrip_alignment_, TRUE);
  } else {
    gtk_notebook_set_show_tabs(GTK_NOTEBOOK(notebook_), FALSE);
    gtk_notebook_set_show_border(GTK_NOTEBOOK(notebook_), FALSE);
    gtk_widget_set_no_show_all(tabstrip_alignment_, FALSE);
    gtk_widget_show_all(tabstrip_alignment_);
  }
}

void WebsiteSettingsPopupGtk::OnPermissionChanged(
    PermissionSelector* selector) {
  presenter_->OnSitePermissionChanged(selector->GetType(),
                                      selector->GetSetting());
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
  close_button_.reset(CustomDrawButton::CloseButtonBubble(theme_service_));
  g_signal_connect(close_button_->widget(), "clicked",
                   G_CALLBACK(OnCloseButtonClickedThunk), this);
  gtk_box_pack_start(GTK_BOX(hbox), close_button_->widget(), FALSE, FALSE, 0);
  int label_width = kPopupWidth - close_button_->SurfaceWidth();
  gtk_util::SetLabelWidth(identity_label, label_width);
  gtk_box_pack_start(GTK_BOX(header_box_), hbox, FALSE, FALSE, 0);

  std::string identity_status_text;
  const GdkColor* color = &ui::kGdkBlack;

  switch (identity_info.identity_status) {
    case WebsiteSettings::SITE_IDENTITY_STATUS_CERT:
    case WebsiteSettings::SITE_IDENTITY_STATUS_EV_CERT:
      identity_status_text =
          l10n_util::GetStringUTF8(IDS_WEBSITE_SETTINGS_IDENTITY_VERIFIED);
      color = &kGdkGreen;
      break;
    default:
      identity_status_text =
          l10n_util::GetStringUTF8(IDS_WEBSITE_SETTINGS_IDENTITY_NOT_VERIFIED);
      break;
  }
  GtkWidget* status_label = CreateTextLabel(
      identity_status_text, kPopupWidth, theme_service_, *color);
  gtk_box_pack_start(
      GTK_BOX(header_box_), status_label, FALSE, FALSE, 0);
  gtk_widget_show_all(header_box_);

  // Create identity section.
  GtkWidget* section_content = gtk_vbox_new(FALSE, ui::kControlSpacing);
  GtkWidget* identity_description =
      CreateTextLabel(identity_info.identity_status_description,
                      kConnectionTabTextWidth, theme_service_, ui::kGdkBlack);
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
                      kConnectionTabTextWidth, theme_service_, ui::kGdkBlack);
  section_content = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(section_content), connection_description, FALSE,
                     FALSE, 0);
  SetConnectionSection(
      connection_contents_,
      WebsiteSettingsUI::GetConnectionIcon(identity_info.connection_status),
      section_content);
}

void WebsiteSettingsPopupGtk::SetFirstVisit(const string16& first_visit) {
  GtkWidget* title = theme_service_->BuildLabel(
      l10n_util::GetStringUTF8(IDS_PAGE_INFO_SITE_INFO_TITLE),
      ui::kGdkBlack);
  PangoAttrList* attributes = pango_attr_list_new();
  pango_attr_list_insert(attributes,
                         pango_attr_weight_new(PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes(GTK_LABEL(title), attributes);
  pango_attr_list_unref(attributes);
  gtk_misc_set_alignment(GTK_MISC(title), 0, 0);

  GtkWidget* first_visit_label = CreateTextLabel(UTF16ToUTF8(first_visit),
                                                 kConnectionTabTextWidth,
                                                 theme_service_,
                                                 ui::kGdkBlack);
  GtkWidget* section_contents = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(section_contents), title, FALSE, FALSE, 0);
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
           web_contents_ ? web_contents_->GetURL() : GURL::EmptyGURL(),
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

int WebsiteSettingsPopupGtk::TabstripButtonToTabIndex(GtkWidget* button) {
  GList* tabs = gtk_container_get_children(GTK_CONTAINER(button->parent));
  int i = 0;
  for (GList* it = tabs; it; it = g_list_next(it), ++i) {
    if (it->data == button)
      break;
  }
  g_list_free(tabs);

  return i;
}

gboolean WebsiteSettingsPopupGtk::OnTabButtonPress(
    GtkWidget* widget, GdkEvent* event) {
  gtk_notebook_set_current_page(GTK_NOTEBOOK(notebook_),
                                TabstripButtonToTabIndex(widget));
  gtk_widget_queue_draw(tabstrip_alignment_);
  return FALSE;
}

gboolean WebsiteSettingsPopupGtk::OnTabExpose(
    GtkWidget* widget, GdkEventExpose* event) {
  if (gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook_)) !=
      TabstripButtonToTabIndex(widget)) {
    return FALSE;
  }

  NineBox nine(IDR_WEBSITE_SETTINGS_TAB_LEFT2,
               IDR_WEBSITE_SETTINGS_TAB_CENTER2,
               IDR_WEBSITE_SETTINGS_TAB_RIGHT2,
               0, 0, 0, 0, 0, 0);
  nine.RenderToWidget(widget);

  return FALSE;
}

gboolean WebsiteSettingsPopupGtk::OnTabstripExpose(
    GtkWidget* widget, GdkEventExpose* event) {
  int tab_idx = gtk_notebook_get_current_page(GTK_NOTEBOOK(notebook_));
  GtkWidget* tabstrip = gtk_bin_get_child(GTK_BIN(tabstrip_alignment_));
  GList* tabs = gtk_container_get_children(GTK_CONTAINER(tabstrip));
  GtkWidget* selected_tab = GTK_WIDGET(g_list_nth_data(tabs, tab_idx));
  g_list_free(tabs);
  GtkAllocation tab_bounds;
  gtk_widget_get_allocation(selected_tab, &tab_bounds);

  cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(gtk_widget_get_window(widget)));
  GtkAllocation allocation;
  gtk_widget_get_allocation(widget, &allocation);
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();

  // Draw the shadows that abut the selected tab.
  gfx::CairoCachedSurface* left_tab_shadow =
      rb.GetNativeImageNamed(IDR_WEBSITE_SETTINGS_TABSTRIP_LEFT).ToCairo();
  int tab_shadow_width = left_tab_shadow->Width();
  left_tab_shadow->SetSource(cr, widget,
      tab_bounds.x - tab_shadow_width, allocation.y);
  cairo_paint(cr);

  gfx::CairoCachedSurface* right_tab_shadow =
      rb.GetNativeImageNamed(IDR_WEBSITE_SETTINGS_TABSTRIP_RIGHT).ToCairo();
  right_tab_shadow->SetSource(cr, widget,
      tab_bounds.x + tab_bounds.width, allocation.y);
  cairo_paint(cr);

  // Draw the shadow for the inactive part of the tabstrip.
  gfx::CairoCachedSurface* tiling_shadow =
      rb.GetNativeImageNamed(IDR_WEBSITE_SETTINGS_TABSTRIP_CENTER).ToCairo();

  tiling_shadow->SetSource(cr, widget, allocation.x, allocation.y);
  cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);

  GdkRectangle left_tiling_area =
      { allocation.x, allocation.y,
        tab_bounds.x - tab_shadow_width, allocation.height };
  GdkRectangle invalid_left_tiling_area;
  if (gdk_rectangle_intersect(&left_tiling_area, &event->area,
                              &invalid_left_tiling_area)) {
    gdk_cairo_rectangle(cr, &invalid_left_tiling_area);
    cairo_fill(cr);
  }

  GdkRectangle right_tiling_area =
      { tab_bounds.x + tab_bounds.width + tab_shadow_width,
        allocation.y,
        allocation.width,
        allocation.height };
  GdkRectangle invalid_right_tiling_area;
  if (gdk_rectangle_intersect(&right_tiling_area, &event->area,
                              &invalid_right_tiling_area)) {
    gdk_cairo_rectangle(cr, &invalid_right_tiling_area);
    cairo_fill(cr);
  }

  cairo_destroy(cr);
  return FALSE;
}

void WebsiteSettingsPopupGtk::OnCookiesLinkClicked(GtkWidget* widget) {
  // Count how often the Collected Cookies dialog is opened.
  content::RecordAction(
      content::UserMetricsAction("WebsiteSettings_CookiesDialogOpened"));

  new CollectedCookiesGtk(GTK_WINDOW(parent_), web_contents_);
  bubble_->Close();
}

void WebsiteSettingsPopupGtk::OnViewCertLinkClicked(GtkWidget* widget) {
  DCHECK_NE(cert_id_, 0);
  ShowCertificateViewerByID(web_contents_, GTK_WINDOW(parent_), cert_id_);
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
