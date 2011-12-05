// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/sad_tab_gtk.h"

#include "base/utf_string_conversions.h"
#include "chrome/browser/google/google_util.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/common/url_constants.h"
#include "content/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/image/image.h"

namespace {

// Background color of the content (a grayish blue) for a crashed tab.
const GdkColor kCrashedBackgroundColor = GDK_COLOR_RGB(35, 48, 64);

// Background color of the content (a grayish purple) for a killed
// tab.  TODO(gspencer): update this for the "real" color when the UI
// team provides one.  See http://crosbug.com/10711
const GdkColor kKilledBackgroundColor = GDK_COLOR_RGB(57, 48, 88);

// Construct a centered GtkLabel with a white foreground.
// |format| is a printf-style format containing a %s;
// |str| is substituted into the format.
GtkWidget* MakeWhiteMarkupLabel(const char* format, const std::string& str) {
  GtkWidget* label = gtk_label_new(NULL);
  char* markup = g_markup_printf_escaped(format, str.c_str());
  gtk_label_set_markup(GTK_LABEL(label), markup);
  g_free(markup);

  // Center align and justify it.
  gtk_misc_set_alignment(GTK_MISC(label), 0.5, 0.5);
  gtk_label_set_justify(GTK_LABEL(label), GTK_JUSTIFY_CENTER);

  // Set text to white.
  GdkColor white = ui::kGdkWhite;
  gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &white);

  return label;
}

}  // namespace

SadTabGtk::SadTabGtk(TabContents* tab_contents, Kind kind)
    : tab_contents_(tab_contents),
      kind_(kind) {
  DCHECK(tab_contents_);

  // Use an event box to get the background painting correctly.
  event_box_.Own(gtk_event_box_new());
  gtk_widget_modify_bg(event_box_.get(), GTK_STATE_NORMAL,
                       kind == CRASHED ?
                       &kCrashedBackgroundColor :
                       &kKilledBackgroundColor);
  // Allow ourselves to be resized arbitrarily small.
  gtk_widget_set_size_request(event_box_.get(), 0, 0);

  GtkWidget* centering = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
  gtk_container_add(GTK_CONTAINER(event_box_.get()), centering);

  // Use a vertical box to contain icon, title, message and link.
  GtkWidget* vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(centering), vbox);

  // Add center-aligned image.
  GtkWidget* image = gtk_image_new_from_pixbuf(
      ResourceBundle::GetSharedInstance().GetNativeImageNamed(
          kind == CRASHED ? IDR_SAD_TAB : IDR_KILLED_TAB));
  gtk_misc_set_alignment(GTK_MISC(image), 0.5, 0.5);
  gtk_box_pack_start(GTK_BOX(vbox), image, FALSE, FALSE, 0);

  // Add spacer between image and title.
  GtkWidget* spacer = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(spacer), "<span size=\"larger\"> </span>");
  gtk_box_pack_start(GTK_BOX(vbox), spacer, FALSE, FALSE, 0);

  // Add center-aligned title.
  GtkWidget* title = MakeWhiteMarkupLabel(
      "<span size=\"larger\" style=\"normal\"><b>%s</b></span>",
      l10n_util::GetStringUTF8(kind == CRASHED ?
                               IDS_SAD_TAB_TITLE :
                               IDS_KILLED_TAB_TITLE));
  gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

  // Add spacer between title and message.
  spacer = gtk_label_new(" ");
  gtk_box_pack_start(GTK_BOX(vbox), spacer, FALSE, FALSE, 0);

  // Add center-aligned message.
  GtkWidget* message = MakeWhiteMarkupLabel(
      "<span style=\"normal\">%s</span>",
      l10n_util::GetStringUTF8(kind == CRASHED ?
                               IDS_SAD_TAB_MESSAGE :
                               IDS_KILLED_TAB_MESSAGE));
  gtk_label_set_line_wrap(GTK_LABEL(message), TRUE);
  gtk_box_pack_start(GTK_BOX(vbox), message, FALSE, FALSE, 0);

  // Add spacer between message and link.
  spacer = gtk_label_new(" ");
  gtk_box_pack_start(GTK_BOX(vbox), spacer, FALSE, FALSE, 0);

  if (tab_contents_ != NULL) {
    // Create the help link and alignment.
    std::string link_text(l10n_util::GetStringUTF8(
        kind == CRASHED ? IDS_SAD_TAB_HELP_LINK : IDS_LEARN_MORE));
    GtkWidget* link = gtk_chrome_link_button_new(link_text.c_str());
    gtk_chrome_link_button_set_normal_color(GTK_CHROME_LINK_BUTTON(link),
                                            &ui::kGdkWhite);
    g_signal_connect(link, "clicked", G_CALLBACK(OnLinkButtonClickThunk), this);
    GtkWidget* help_alignment = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);

    if (kind == CRASHED) {
      // Use a horizontal box to contain the help text and link.
      GtkWidget* help_hbox = gtk_hbox_new(FALSE, 0);
      gtk_container_add(GTK_CONTAINER(vbox), help_hbox);

      size_t offset = 0;
      string16 help_text(l10n_util::GetStringFUTF16(IDS_SAD_TAB_HELP_MESSAGE,
                                                    string16(), &offset));
      std::string help_prefix_text(UTF16ToUTF8(help_text.substr(0, offset)));
      std::string help_suffix_text(UTF16ToUTF8(help_text.substr(offset)));

      GtkWidget* help_prefix = MakeWhiteMarkupLabel(
          "<span style=\"normal\">%s</span>", help_prefix_text);
      GtkWidget* help_suffix = MakeWhiteMarkupLabel(
          "<span style=\"normal\">%s</span>", help_suffix_text);

      // Add the help link and text to the horizontal box.
      gtk_box_pack_start(GTK_BOX(help_hbox), help_prefix, FALSE, FALSE, 0);
      GtkWidget* link_alignment = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
      gtk_container_add(GTK_CONTAINER(link_alignment), link);
      gtk_box_pack_start(GTK_BOX(help_hbox), link_alignment, FALSE, FALSE, 0);
      gtk_box_pack_start(GTK_BOX(help_hbox), help_suffix, FALSE, FALSE, 0);
    } else {
      // Add just the help link to a centered alignment.
      gtk_container_add(GTK_CONTAINER(help_alignment), link);
    }
    gtk_box_pack_start(GTK_BOX(vbox), help_alignment, FALSE, FALSE, 0);
  }

  gtk_widget_show_all(event_box_.get());
}

SadTabGtk::~SadTabGtk() {
  event_box_.Destroy();
}

void SadTabGtk::OnLinkButtonClick(GtkWidget* sender) {
  if (tab_contents_ != NULL) {
    GURL help_url =
        google_util::AppendGoogleLocaleParam(GURL(
            kind_ == CRASHED ?
            chrome::kCrashReasonURL :
            chrome::kKillReasonURL));
    tab_contents_->OpenURL(OpenURLParams(
        help_url, content::Referrer(), CURRENT_TAB,
        content::PAGE_TRANSITION_LINK, false));
  }
}
