// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/website_settings_popup_gtk.h"

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/string_number_conversions.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/collected_cookies_gtk.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/theme_service_gtk.h"
#include "chrome/browser/ui/gtk/location_bar_view_gtk.h"
#include "chrome/browser/ui/tab_contents/tab_contents_wrapper.h"
#include "chrome/common/url_constants.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"

using content::OpenURLParams;

WebsiteSettingsPopupGtk::WebsiteSettingsPopupGtk(
    gfx::NativeWindow parent,
    Profile* profile,
    TabContentsWrapper* tab_contents_wrapper)
    : parent_(parent),
      contents_(NULL),
      theme_service_(ThemeServiceGtk::GetFrom(profile)),
      profile_(profile),
      tab_contents_wrapper_(tab_contents_wrapper),
      browser_(NULL),
      site_info_contents_(NULL),
      cookies_section_contents_(NULL),
      permissions_section_contents_(NULL),
      delegate_(NULL) {
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
                            true,  // |match_system_theme|
                            true,  // |grab_input|
                            theme_service_,
                            this);  // |delegate|
  if (!bubble_) {
    NOTREACHED();
    return;
  }
}

WebsiteSettingsPopupGtk::~WebsiteSettingsPopupGtk() {
}

void WebsiteSettingsPopupGtk::SetDelegate(WebsiteSettingsUIDelegate* delegate) {
  delegate_ = delegate;
}

void WebsiteSettingsPopupGtk::BubbleClosing(BubbleGtk* bubble,
                                            bool closed_by_escape) {
  if (delegate_)
    delegate_->OnUIClosing();
}

void WebsiteSettingsPopupGtk::InitContents() {
  if (!contents_) {
    contents_ = gtk_vbox_new(FALSE, ui::kContentAreaSpacing);
    gtk_container_set_border_width(GTK_CONTAINER(contents_),
                                   ui::kContentAreaBorder);
  } else {
    gtk_util::RemoveAllChildren(contents_);
  }

  site_info_contents_ = gtk_vbox_new(FALSE, ui::kControlSpacing);
  std::string title =
      l10n_util::GetStringUTF8(IDS_PAGE_INFO_SITE_INFO_TITLE);
  gtk_box_pack_start(GTK_BOX(contents_),
                     CreateSection(title, site_info_contents_),
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(contents_),
                     gtk_hseparator_new(),
                     FALSE, FALSE, 0);
  cookies_section_contents_ = gtk_vbox_new(FALSE, ui::kControlSpacing);
  title = l10n_util::GetStringUTF8(IDS_WEBSITE_SETTINGS_TITLE_SITE_DATA);
  gtk_box_pack_start(GTK_BOX(contents_),
                     CreateSection(title,
                                   cookies_section_contents_),
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(contents_),
                     gtk_hseparator_new(),
                     FALSE, FALSE, 0);
  permissions_section_contents_ = gtk_vbox_new(FALSE, ui::kControlSpacing);
  title = l10n_util::GetStringUTF8(IDS_WEBSITE_SETTINGS_TITLE_SITE_PERMISSIONS);
  gtk_box_pack_start(GTK_BOX(contents_),
                     CreateSection(title,
                                   permissions_section_contents_),
                     FALSE, FALSE, 0);

  gtk_widget_show_all(contents_);
}

void WebsiteSettingsPopupGtk::ClearContainer(GtkWidget* container) {
  GList* child = gtk_container_get_children(GTK_CONTAINER(container));
  while (child) {
    gtk_container_remove(GTK_CONTAINER(container), GTK_WIDGET(child->data));
    child = child->next;
  }
}

void WebsiteSettingsPopupGtk::SetSiteInfo(const std::string site_info) {
  DCHECK(site_info_contents_);
  ClearContainer(site_info_contents_);
  GtkWidget* label = theme_service_->BuildLabel(site_info,
                                                ui::kGdkBlack);
  GtkWidget* site_info_entry_box = gtk_hbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(site_info_entry_box), label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(site_info_contents_), site_info_entry_box, FALSE,
                     FALSE, 0);
  gtk_widget_show_all(site_info_contents_);
}

GtkWidget* WebsiteSettingsPopupGtk::CreateSection(std::string section_title,
                                                  GtkWidget* section_content) {
  GtkWidget* section_box = gtk_vbox_new(FALSE, ui::kControlSpacing);

  // Add Section title
  GtkWidget* title_hbox = gtk_hbox_new(FALSE, ui::kControlSpacing);

  GtkWidget* label = theme_service_->BuildLabel(section_title,
                                                ui::kGdkBlack);
  gtk_label_set_selectable(GTK_LABEL(label), TRUE);
  PangoAttrList* attributes = pango_attr_list_new();
  pango_attr_list_insert(attributes,
                         pango_attr_weight_new(PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes(GTK_LABEL(label), attributes);
  pango_attr_list_unref(attributes);
  gtk_util::SetLabelWidth(label, 400);
  gtk_box_pack_start(GTK_BOX(section_box), title_hbox, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(title_hbox), label, FALSE, FALSE, 0);

  // Add section content
  gtk_box_pack_start(GTK_BOX(section_box), section_content, FALSE, FALSE, 0);
  return section_box;

}

void WebsiteSettingsPopupGtk::SetCookieInfo(
    const CookieInfoList& cookie_info_list) {
  DCHECK(cookies_section_contents_);
  ClearContainer(cookies_section_contents_);

  // Create cookies info rows.
  for (CookieInfoList::const_iterator it = cookie_info_list.begin();
       it != cookie_info_list.end();
       ++it) {
    GtkWidget* cookies_info = gtk_hbox_new(FALSE, 0);

    GtkWidget* label = theme_service_->BuildLabel(it->text, ui::kGdkBlack);
    gtk_label_set_selectable(GTK_LABEL(label), TRUE);
    gtk_util::SetLabelWidth(label, 200);
    // Allow linebreaking in the middle of words if necessary, so that extremely
    // long hostnames (longer than one line) will still be completely shown.
    gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
    gtk_box_pack_start(GTK_BOX(cookies_info), label, FALSE, FALSE, 0);

    std::string allowed_count = base::IntToString(it->allowed);
    std::string blocked_count = base::IntToString(it->blocked);
    // TODO(markusheintz): Add a localized label here once we decided how this
    // information should be displayed.
    std::string info_str = " (" + allowed_count + " allowed / "
                           + blocked_count + " blocked)";

    GtkWidget* info = theme_service_->BuildLabel(info_str, ui::kGdkBlack);
    gtk_label_set_selectable(GTK_LABEL(info), TRUE);
    gtk_box_pack_start(GTK_BOX(cookies_info), info, FALSE, FALSE, 0);

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

void WebsiteSettingsPopupGtk::SetPermissionInfo(
      const PermissionInfoList& permission_info_list) {
  DCHECK(permissions_section_contents_);
  ClearContainer(permissions_section_contents_);

  for (PermissionInfoList::const_iterator permission =
           permission_info_list.begin();
       permission != permission_info_list.end();
       ++permission) {
    std::string permission_text;
    switch (permission->type) {
      case CONTENT_SETTINGS_TYPE_POPUPS:
        permission_text =
            l10n_util::GetStringUTF8(IDS_WEBSITE_SETTINGS_TYPE_POPUPS);
        break;
      case CONTENT_SETTINGS_TYPE_PLUGINS:
        permission_text =
            l10n_util::GetStringUTF8(IDS_WEBSITE_SETTINGS_TYPE_PLUGINS);
        break;
      case CONTENT_SETTINGS_TYPE_GEOLOCATION:
        permission_text =
            l10n_util::GetStringUTF8(IDS_WEBSITE_SETTINGS_TYPE_LOCATION);
        break;
      case CONTENT_SETTINGS_TYPE_NOTIFICATIONS:
        permission_text =
            l10n_util::GetStringUTF8(IDS_WEBSITE_SETTINGS_TYPE_NOTIFICATIONS);
        break;
      default:
        NOTREACHED();
        break;
    }
    GtkWidget* label = theme_service_->BuildLabel(permission_text,
                                                  ui::kGdkBlack);
    gtk_util::SetLabelWidth(label, 280);
    GtkWidget* hbox = gtk_hbox_new(FALSE, 0);
    gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

    GtkWidget* combo_box = gtk_combo_box_new_text();
    std::string permission_str =
        l10n_util::GetStringUTF8(IDS_WEBSITE_SETTINGS_PERMISSION_ALLOW);
    gtk_combo_box_append_text(GTK_COMBO_BOX(combo_box), permission_str.c_str());
    permission_str =
        l10n_util::GetStringUTF8(IDS_WEBSITE_SETTINGS_PERMISSION_BLOCK);
    gtk_combo_box_append_text(GTK_COMBO_BOX(combo_box), permission_str.c_str());
    gtk_combo_box_set_active(
        GTK_COMBO_BOX(combo_box),
        permission->setting == CONTENT_SETTING_ALLOW ? 0 : 1);
    g_signal_connect(combo_box, "changed",
                     G_CALLBACK(OnPermissionChangedThunk), this);
    g_signal_connect(combo_box, "notify::popup-shown",
                     G_CALLBACK(OnComboBoxShownThunk), this);
    gtk_box_pack_start(GTK_BOX(hbox), combo_box, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(permissions_section_contents_), hbox, FALSE,
                       FALSE, 0);
  }

  GtkWidget* show_content_settings_link = theme_service_->BuildChromeLinkButton(
      l10n_util::GetStringUTF8(IDS_WEBSITE_SETTINGS_SHOW_PERMISSION_SETTINGS));
  g_signal_connect(show_content_settings_link, "clicked",
                   G_CALLBACK(OnPermissionsSettingsLinkClickedThunk), this);
  GtkWidget* link_hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(link_hbox), show_content_settings_link,
                     FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(permissions_section_contents_), link_hbox,
                     FALSE, FALSE, 0);

  gtk_widget_show_all(permissions_section_contents_);
}

void WebsiteSettingsPopupGtk::OnComboBoxShown(GtkWidget* widget,
                                              GParamSpec* property) {
  // GtkComboBox grabs the keyboard and pointer when it displays its popup,
  // which steals the grabs that BubbleGtk had installed.  When the popup is
  // hidden, we notify BubbleGtk so it can try to reacquire the grabs
  // (otherwise, GTK won't activate our widgets when the user clicks in them).
  gboolean popup_shown = FALSE;
  g_object_get(G_OBJECT(GTK_COMBO_BOX(widget)), "popup-shown", &popup_shown,
               NULL);
  if (!popup_shown)
    bubble_->HandlePointerAndKeyboardUngrabbedByContent();
}

void WebsiteSettingsPopupGtk::OnCookiesLinkClicked(GtkWidget* widget) {
  new CollectedCookiesGtk(GTK_WINDOW(parent_),
                          tab_contents_wrapper_);
  bubble_->Close();
}

void WebsiteSettingsPopupGtk::OnPermissionsSettingsLinkClicked(
    GtkWidget* widget) {
  browser_->OpenURL(OpenURLParams(
      GURL(std::string(
          chrome::kChromeUISettingsURL) + chrome::kContentSettingsSubPage),
      content::Referrer(),
      NEW_FOREGROUND_TAB,
      content::PAGE_TRANSITION_LINK,
      false));
  bubble_->Close();
}

void WebsiteSettingsPopupGtk::OnPermissionChanged(GtkWidget* widget) {
  // TODO(markusheintz): Implement once the backend (|WebsiteSettingsUIDelegate|
  // implmentation which is the |WebsiteSettings| class) provides support for
  // this.
}
