// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <gtk/gtk.h>

#include "base/basictypes.h"
#include "base/compiler_specific.h"
#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/certificate_viewer.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/page_info_model.h"
#include "chrome/browser/page_info_model_observer.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/bubble/bubble_gtk.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/location_bar_view_gtk.h"
#include "chrome/common/url_constants.h"
#include "googleurl/src/gurl.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"

class Profile;

namespace {

class PageInfoBubbleGtk : public PageInfoModelObserver,
                          public BubbleDelegateGtk {
 public:
  PageInfoBubbleGtk(gfx::NativeWindow parent,
                    Profile* profile,
                    const GURL& url,
                    const NavigationEntry::SSLStatus& ssl,
                    bool show_history);
  virtual ~PageInfoBubbleGtk();

  // PageInfoModelObserver implementation.
  virtual void OnPageInfoModelChanged() OVERRIDE;

  // BubbleDelegateGtk implementation.
  virtual void BubbleClosing(BubbleGtk* bubble, bool closed_by_escape) OVERRIDE;

 private:
  // Layouts the different sections retrieved from the model.
  void InitContents();

  // Returns a widget that contains the UI for the passed |section|.
  GtkWidget* CreateSection(const PageInfoModel::SectionInfo& section);

  // Link button callbacks.
  CHROMEGTK_CALLBACK_0(PageInfoBubbleGtk, void, OnViewCertLinkClicked);
  CHROMEGTK_CALLBACK_0(PageInfoBubbleGtk, void, OnHelpLinkClicked);

  // The model containing the different sections to display.
  PageInfoModel model_;

  // The url for this dialog. Should be unique among active dialogs.
  GURL url_;

  // The id of the certificate for this page.
  int cert_id_;

  // Parent window.
  GtkWindow* parent_;

  // The virtual box containing the sections.
  GtkWidget* contents_;

  // The widget relative to which we are positioned.
  GtkWidget* anchor_;

  // Provides colors and stuff.
  GtkThemeService* theme_service_;

  BubbleGtk* bubble_;

  Profile* profile_;

  DISALLOW_COPY_AND_ASSIGN(PageInfoBubbleGtk);
};

PageInfoBubbleGtk::PageInfoBubbleGtk(gfx::NativeWindow parent,
                                     Profile* profile,
                                     const GURL& url,
                                     const NavigationEntry::SSLStatus& ssl,
                                     bool show_history)
    : ALLOW_THIS_IN_INITIALIZER_LIST(model_(profile, url, ssl,
                                            show_history, this)),
      url_(url),
      cert_id_(ssl.cert_id()),
      parent_(parent),
      contents_(NULL),
      theme_service_(GtkThemeService::GetFrom(profile)),
      profile_(profile) {
  BrowserWindowGtk* browser_window =
      BrowserWindowGtk::GetBrowserWindowForNativeWindow(parent);

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

PageInfoBubbleGtk::~PageInfoBubbleGtk() {
}

void PageInfoBubbleGtk::OnPageInfoModelChanged() {
  InitContents();
}

void PageInfoBubbleGtk::BubbleClosing(BubbleGtk* bubble,
                                      bool closed_by_escape) {
  delete this;
}

void PageInfoBubbleGtk::InitContents() {
  if (!contents_) {
    contents_ = gtk_vbox_new(FALSE, ui::kContentAreaSpacing);
    gtk_container_set_border_width(GTK_CONTAINER(contents_),
                                   ui::kContentAreaBorder);
  } else {
    gtk_util::RemoveAllChildren(contents_);
  }

  for (int i = 0; i < model_.GetSectionCount(); i++) {
    gtk_box_pack_start(GTK_BOX(contents_),
                       CreateSection(model_.GetSectionInfo(i)),
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(contents_),
                       gtk_hseparator_new(),
                       FALSE, FALSE, 0);
  }

  GtkWidget* help_link = theme_service_->BuildChromeLinkButton(
      l10n_util::GetStringUTF8(IDS_PAGE_INFO_HELP_CENTER_LINK));
  GtkWidget* help_link_hbox = gtk_hbox_new(FALSE, 0);
  // Stick it in an hbox so it doesn't expand to the whole width.
  gtk_box_pack_start(GTK_BOX(help_link_hbox), help_link, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(contents_), help_link_hbox, FALSE, FALSE, 0);
  g_signal_connect(help_link, "clicked",
                   G_CALLBACK(OnHelpLinkClickedThunk), this);

  gtk_widget_show_all(contents_);
}

GtkWidget* PageInfoBubbleGtk::CreateSection(
    const PageInfoModel::SectionInfo& section) {
  GtkWidget* section_box = gtk_hbox_new(FALSE, ui::kControlSpacing);

  GdkPixbuf* pixbuf = *model_.GetIconImage(section.icon_id);
  if (pixbuf) {
    GtkWidget* image = gtk_image_new_from_pixbuf(pixbuf);
    gtk_box_pack_start(GTK_BOX(section_box), image, FALSE, FALSE, 0);
    gtk_misc_set_alignment(GTK_MISC(image), 0, 0);
  }

  GtkWidget* vbox = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(section_box), vbox, TRUE, TRUE, 0);

  if (!section.headline.empty()) {
    GtkWidget* label = theme_service_->BuildLabel(
        UTF16ToUTF8(section.headline), ui::kGdkBlack);
    gtk_label_set_selectable(GTK_LABEL(label), TRUE);
    PangoAttrList* attributes = pango_attr_list_new();
    pango_attr_list_insert(attributes,
                           pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes(GTK_LABEL(label), attributes);
    pango_attr_list_unref(attributes);
    gtk_util::SetLabelWidth(label, 400);
    // Allow linebreaking in the middle of words if necessary, so that extremely
    // long hostnames (longer than one line) will still be completely shown.
    gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
    gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);
  }
  GtkWidget* label = theme_service_->BuildLabel(
      UTF16ToUTF8(section.description), ui::kGdkBlack);
  gtk_label_set_selectable(GTK_LABEL(label), TRUE);
  gtk_util::SetLabelWidth(label, 400);
  // Allow linebreaking in the middle of words if necessary, so that extremely
  // long hostnames (longer than one line) will still be completely shown.
  gtk_label_set_line_wrap_mode(GTK_LABEL(label), PANGO_WRAP_WORD_CHAR);
  gtk_box_pack_start(GTK_BOX(vbox), label, FALSE, FALSE, 0);

  if (section.type == PageInfoModel::SECTION_INFO_IDENTITY && cert_id_ > 0) {
    GtkWidget* view_cert_link = theme_service_->BuildChromeLinkButton(
        l10n_util::GetStringUTF8(IDS_PAGEINFO_CERT_INFO_BUTTON));
    GtkWidget* cert_link_hbox = gtk_hbox_new(FALSE, 0);
    // Stick it in an hbox so it doesn't expand to the whole width.
    gtk_box_pack_start(GTK_BOX(cert_link_hbox), view_cert_link,
                       FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), cert_link_hbox, FALSE, FALSE, 0);
    g_signal_connect(view_cert_link, "clicked",
                     G_CALLBACK(OnViewCertLinkClickedThunk), this);
  }

  return section_box;
}

void PageInfoBubbleGtk::OnViewCertLinkClicked(GtkWidget* widget) {
  ShowCertificateViewerByID(GTK_WINDOW(parent_), cert_id_);
  bubble_->Close();
}

void PageInfoBubbleGtk::OnHelpLinkClicked(GtkWidget* widget) {
  GURL url = google_util::AppendGoogleLocaleParam(
      GURL(chrome::kPageInfoHelpCenterURL));
  Browser* browser = BrowserList::GetLastActiveWithProfile(profile_);
  browser->OpenURL(OpenURLParams(
      url, content::Referrer(), NEW_FOREGROUND_TAB,
      content::PAGE_TRANSITION_LINK, false));
  bubble_->Close();
}

}  // namespace

namespace browser {

void ShowPageInfoBubble(gfx::NativeWindow parent,
                        Profile* profile,
                        const GURL& url,
                        const NavigationEntry::SSLStatus& ssl,
                        bool show_history) {
  new PageInfoBubbleGtk(parent, profile, url, ssl, show_history);
}

}  // namespace browser
