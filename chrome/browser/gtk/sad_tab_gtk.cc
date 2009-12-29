// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/sad_tab_gtk.h"

#include <string>

#include "app/gfx/gtk_util.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/lazy_instance.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/tab_contents/tab_contents.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

namespace {

// Background color of the content (a grayish blue).
const GdkColor kBackgroundColor = GDK_COLOR_RGB(35, 48, 64);

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
  GdkColor white = gfx::kGdkWhite;
  gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &white);

  return label;
}

}  // namespace

SadTabGtk::SadTabGtk(TabContents* tab_contents)
  : tab_contents_(tab_contents) {
  DCHECK(tab_contents_);

  // Use an event box to get the background painting correctly.
  event_box_.Own(gtk_event_box_new());
  gtk_widget_modify_bg(event_box_.get(), GTK_STATE_NORMAL, &kBackgroundColor);
  // Allow ourselves to be resized arbitrarily small.
  gtk_widget_set_size_request(event_box_.get(), 0, 0);

  GtkWidget* centering = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
  gtk_container_add(GTK_CONTAINER(event_box_.get()), centering);

  // Use a vertical box to contain icon, title, message and link.
  GtkWidget* vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(centering), vbox);

  // Add center-aligned image.
  GtkWidget* image = gtk_image_new_from_pixbuf(
      ResourceBundle::GetSharedInstance().GetPixbufNamed(IDR_SAD_TAB));
  gtk_misc_set_alignment(GTK_MISC(image), 0.5, 0.5);
  gtk_box_pack_start(GTK_BOX(vbox), image, FALSE, FALSE, 0);

  // Add spacer between image and title.
  GtkWidget* spacer = gtk_label_new(NULL);
  gtk_label_set_markup(GTK_LABEL(spacer), "<span size=\"larger\"> </span>");
  gtk_box_pack_start(GTK_BOX(vbox), spacer, FALSE, FALSE, 0);

  // Add center-aligned title.
  GtkWidget* title = MakeWhiteMarkupLabel(
      "<span size=\"larger\" style=\"normal\"><b>%s</b></span>",
      l10n_util::GetStringUTF8(IDS_SAD_TAB_TITLE));
  gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, 0);

  // Add spacer between title and message.
  spacer = gtk_label_new(" ");
  gtk_box_pack_start(GTK_BOX(vbox), spacer, FALSE, FALSE, 0);

  // Add center-aligned message.
  GtkWidget* message =
      MakeWhiteMarkupLabel("<span style=\"normal\">%s</span>",
                           l10n_util::GetStringUTF8(IDS_SAD_TAB_MESSAGE));
  gtk_label_set_line_wrap(GTK_LABEL(message), TRUE);
  gtk_box_pack_start(GTK_BOX(vbox), message, FALSE, FALSE, 0);

  // Add spacer between message and link.
  spacer = gtk_label_new(" ");
  gtk_box_pack_start(GTK_BOX(vbox), spacer, FALSE, FALSE, 0);

  if (tab_contents_ != NULL) {
    // Add the learn-more link and center-align it in an alignment.
    GtkWidget* link = gtk_chrome_link_button_new(
        l10n_util::GetStringUTF8(IDS_LEARN_MORE).c_str());
    gtk_chrome_link_button_set_normal_color(GTK_CHROME_LINK_BUTTON(link),
                                            &gfx::kGdkWhite);
    g_signal_connect(link, "clicked", G_CALLBACK(OnLinkButtonClickThunk), this);
    GtkWidget* link_alignment = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
    gtk_container_add(GTK_CONTAINER(link_alignment), link);
    gtk_box_pack_start(GTK_BOX(vbox), link_alignment, FALSE, FALSE, 0);
  }

  gtk_widget_show_all(event_box_.get());
}

SadTabGtk::~SadTabGtk() {
  event_box_.Destroy();
}

void SadTabGtk::OnLinkButtonClick() {
  if (tab_contents_ != NULL) {
    tab_contents_->OpenURL(GURL(l10n_util::GetStringUTF8(IDS_CRASH_REASON_URL)),
        GURL(), CURRENT_TAB, PageTransition::LINK);
  }
}
