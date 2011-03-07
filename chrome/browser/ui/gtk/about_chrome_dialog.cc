// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/about_chrome_dialog.h"

#include <gtk/gtk.h>

#include <string>
#include <vector>

#include "base/utf_string_conversions.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/gtk/cairo_cached_surface.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_provider.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/common/chrome_constants.h"
#include "chrome/common/chrome_version_info.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "webkit/glue/webkit_glue.h"

namespace {

// Left or right margin.
const int kPanelHorizMargin = 13;

// Top or bottom margin.
const int kPanelVertMargin = 20;

// Extra spacing between product name and version number.
const int kExtraLineSpacing = 5;

// These are used as placeholder text around the links in the text in the about
// dialog.
const char* kBeginLinkChr = "BEGIN_LINK_CHR";
const char* kBeginLinkOss = "BEGIN_LINK_OSS";
// We don't actually use this one.
// const char* kEndLinkChr = "END_LINK_CHR";
const char* kEndLinkOss = "END_LINK_OSS";
const char* kBeginLink = "BEGIN_LINK";
const char* kEndLink = "END_LINK";

void OnResponse(GtkWidget* dialog, int response_id) {
  // We're done.
  gtk_widget_destroy(dialog);
}

GtkWidget* MakeMarkupLabel(const char* format, const std::string& str) {
  GtkWidget* label = gtk_label_new(NULL);
  char* markup = g_markup_printf_escaped(format, str.c_str());
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);

  // Left align it.
  gtk_misc_set_alignment(GTK_MISC(label), 0.0, 0.5);

  return label;
}

void OnLinkButtonClick(GtkWidget* button, const char* url) {
  BrowserList::GetLastActive()->
      OpenURL(GURL(url), GURL(), NEW_WINDOW, PageTransition::LINK);
}

const char* GetChromiumUrl() {
  static GURL url = google_util::AppendGoogleLocaleParam(
      GURL(chrome::kChromiumProjectURL));
  return url.spec().c_str();
}

gboolean OnEventBoxExpose(GtkWidget* event_box,
                          GdkEventExpose* expose,
                          gboolean user_data) {
  cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(event_box->window));
  gdk_cairo_rectangle(cr, &expose->area);
  cairo_clip(cr);
  GtkThemeProvider* theme_provider =
      GtkThemeProvider::GetFrom(BrowserList::GetLastActive()->profile());
  CairoCachedSurface* background = theme_provider->GetSurfaceNamed(
      IDR_ABOUT_BACKGROUND_COLOR, event_box);

  background->SetSource(cr, 0, 0);
  cairo_pattern_set_extend(cairo_get_source(cr), CAIRO_EXTEND_REPEAT);
  gdk_cairo_rectangle(cr, &expose->area);
  cairo_fill(cr);
  cairo_destroy(cr);
  return FALSE;
}

}  // namespace

void ShowAboutDialogForProfile(GtkWindow* parent, Profile* profile) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  static GdkPixbuf* background = rb.GetPixbufNamed(IDR_ABOUT_BACKGROUND);
  chrome::VersionInfo version_info;
  std::string current_version = version_info.Version();
#if !defined(GOOGLE_CHROME_BUILD)
  current_version += " (";
  current_version += version_info.LastChange();
  current_version += ")";
#endif
  std::string channel = platform_util::GetVersionStringModifier();
  if (!channel.empty())
    current_version += " " + channel;

  // Build the dialog.
  GtkWidget* dialog = gtk_dialog_new_with_buttons(
      l10n_util::GetStringUTF8(IDS_ABOUT_CHROME_TITLE).c_str(),
      parent,
      GTK_DIALOG_MODAL,
      NULL);
  // Pick up the style set in gtk_util.cc:InitRCStyles().
  // The layout of this dialog is special because the logo should be flush
  // with the edges of the window.
  gtk_widget_set_name(dialog, "about-dialog");
  gtk_dialog_set_has_separator(GTK_DIALOG(dialog), FALSE);

  GtkWidget* close_button = gtk_dialog_add_button(GTK_DIALOG(dialog),
      GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE);

  GtkWidget* content_area = GTK_DIALOG(dialog)->vbox;

  // Use an event box to get the background painting correctly
  GtkWidget* ebox = gtk_event_box_new();
  gtk_widget_set_app_paintable(ebox, TRUE);
  g_signal_connect(ebox, "expose-event", G_CALLBACK(OnEventBoxExpose), NULL);

  GtkWidget* hbox = gtk_hbox_new(FALSE, 0);

  GtkWidget* text_alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(text_alignment),
                            kPanelVertMargin, kPanelVertMargin,
                            kPanelHorizMargin, kPanelHorizMargin);

  GtkWidget* text_vbox = gtk_vbox_new(FALSE, kExtraLineSpacing);

  GdkColor black = gtk_util::kGdkBlack;
  GtkWidget* product_label = MakeMarkupLabel(
      "<span font_desc=\"18\" style=\"normal\">%s</span>",
      l10n_util::GetStringUTF8(IDS_PRODUCT_NAME));
  gtk_widget_modify_fg(product_label, GTK_STATE_NORMAL, &black);
  gtk_box_pack_start(GTK_BOX(text_vbox), product_label, FALSE, FALSE, 0);

  GtkWidget* version_label = gtk_label_new(current_version.c_str());
  gtk_misc_set_alignment(GTK_MISC(version_label), 0.0, 0.5);
  gtk_label_set_selectable(GTK_LABEL(version_label), TRUE);
  gtk_widget_modify_fg(version_label, GTK_STATE_NORMAL, &black);
  gtk_box_pack_start(GTK_BOX(text_vbox), version_label, FALSE, FALSE, 0);

  gtk_container_add(GTK_CONTAINER(text_alignment), text_vbox);
  gtk_box_pack_start(GTK_BOX(hbox), text_alignment, TRUE, TRUE, 0);

  GtkWidget* image_vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_end(GTK_BOX(image_vbox),
                   gtk_image_new_from_pixbuf(background),
                   FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(hbox), image_vbox, FALSE, FALSE, 0);
  gtk_container_add(GTK_CONTAINER(ebox), hbox);
  gtk_box_pack_start(GTK_BOX(content_area), ebox, TRUE, TRUE, 0);

  // We use a separate box for the licensing etc. text.  See the comment near
  // the top of this function about using a special layout for this dialog.
  GtkWidget* vbox = gtk_vbox_new(FALSE, 0);

  GtkWidget* copyright_label = gtk_label_new(
      l10n_util::GetStringUTF8(IDS_ABOUT_VERSION_COPYRIGHT).c_str());
  gtk_misc_set_alignment(GTK_MISC(copyright_label), 0.0, 0.5);
  gtk_box_pack_start(GTK_BOX(vbox), copyright_label, FALSE, FALSE, 5);

  std::string license = l10n_util::GetStringUTF8(IDS_ABOUT_VERSION_LICENSE);
  bool chromium_url_appears_first =
      license.find(kBeginLinkChr) < license.find(kBeginLinkOss);
  size_t link1 = license.find(kBeginLink);
  DCHECK(link1 != std::string::npos);
  size_t link1_end = license.find(kEndLink, link1);
  DCHECK(link1_end != std::string::npos);
  size_t link2 = license.find(kBeginLink, link1_end);
  DCHECK(link2 != std::string::npos);
  size_t link2_end = license.find(kEndLink, link2);
  DCHECK(link1_end != std::string::npos);

  GtkWidget* license_chunk1 = gtk_label_new(license.substr(0, link1).c_str());
  gtk_misc_set_alignment(GTK_MISC(license_chunk1), 0.0, 0.5);
  GtkWidget* license_chunk2 = gtk_label_new(
      license.substr(link1_end + strlen(kEndLinkOss),
                     link2 - link1_end - strlen(kEndLinkOss)).c_str());
  gtk_misc_set_alignment(GTK_MISC(license_chunk2), 0.0, 0.5);
  GtkWidget* license_chunk3 = gtk_label_new(
      license.substr(link2_end + strlen(kEndLinkOss)).c_str());
  gtk_misc_set_alignment(GTK_MISC(license_chunk3), 0.0, 0.5);

  std::string first_link_text =
      license.substr(link1 + strlen(kBeginLinkOss),
                     link1_end - link1 - strlen(kBeginLinkOss));
  std::string second_link_text =
      license.substr(link2 + strlen(kBeginLinkOss),
                     link2_end - link2 - strlen(kBeginLinkOss));

  GtkWidget* first_link = gtk_chrome_link_button_new(first_link_text.c_str());
  GtkWidget* second_link = gtk_chrome_link_button_new(second_link_text.c_str());
  if (!chromium_url_appears_first) {
    GtkWidget* swap = second_link;
    second_link = first_link;
    first_link = swap;
  }

  g_signal_connect(chromium_url_appears_first ? first_link : second_link,
                   "clicked", G_CALLBACK(OnLinkButtonClick),
                   const_cast<char*>(GetChromiumUrl()));
  g_signal_connect(chromium_url_appears_first ? second_link : first_link,
                   "clicked", G_CALLBACK(OnLinkButtonClick),
                   const_cast<char*>(chrome::kAboutCreditsURL));

  GtkWidget* license_hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(license_hbox), license_chunk1,
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(license_hbox), first_link,
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(license_hbox), license_chunk2,
                     FALSE, FALSE, 0);

  // Since there's no good way to dynamically wrap the license block, force
  // a line break right before the second link (which matches en-US Windows
  // chromium).
  GtkWidget* license_hbox2 = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(license_hbox2), second_link,
                     FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(license_hbox2), license_chunk3,
                     FALSE, FALSE, 0);

  GtkWidget* license_vbox = gtk_vbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(license_vbox), license_hbox, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(license_vbox), license_hbox2, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(vbox), license_vbox, FALSE, FALSE, 0);

#if defined(GOOGLE_CHROME_BUILD)
  // Spacing line.
  gtk_box_pack_start(GTK_BOX(vbox), gtk_label_new(""), FALSE, FALSE, 0);

  std::vector<size_t> url_offsets;
  string16 text = l10n_util::GetStringFUTF16(IDS_ABOUT_TERMS_OF_SERVICE,
                                             string16(),
                                             string16(),
                                             &url_offsets);

  GtkWidget* tos_chunk1 = gtk_label_new(
      UTF16ToUTF8(text.substr(0, url_offsets[0])).c_str());
  gtk_misc_set_alignment(GTK_MISC(tos_chunk1), 0.0, 0.5);
  GtkWidget* tos_link = gtk_chrome_link_button_new(
      l10n_util::GetStringUTF8(IDS_TERMS_OF_SERVICE).c_str());
  GtkWidget* tos_chunk2 = gtk_label_new(
      UTF16ToUTF8(text.substr(url_offsets[0])).c_str());
  gtk_misc_set_alignment(GTK_MISC(tos_chunk2), 0.0, 0.5);

  GtkWidget* tos_hbox = gtk_hbox_new(FALSE, 0);
  gtk_box_pack_start(GTK_BOX(tos_hbox), tos_chunk1, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(tos_hbox), tos_link, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(tos_hbox), tos_chunk2, FALSE, FALSE, 0);

  g_signal_connect(tos_link, "clicked", G_CALLBACK(OnLinkButtonClick),
    const_cast<char*>(chrome::kAboutTermsURL));
  gtk_box_pack_start(GTK_BOX(vbox), tos_hbox, TRUE, TRUE, 0);
#endif

  GtkWidget* alignment = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_alignment_set_padding(GTK_ALIGNMENT(alignment),
      gtk_util::kContentAreaBorder, 0,
      gtk_util::kContentAreaBorder, gtk_util::kContentAreaBorder);
  gtk_container_add(GTK_CONTAINER(alignment), vbox);
  gtk_box_pack_start(GTK_BOX(content_area), alignment, FALSE, FALSE, 0);

  g_signal_connect(dialog, "response", G_CALLBACK(OnResponse), NULL);
  gtk_window_set_resizable(GTK_WINDOW(dialog), FALSE);
  gtk_widget_show_all(dialog);
  gtk_widget_grab_focus(close_button);
}
