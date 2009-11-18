// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/sad_tab_gtk.h"

#include <string>

#include "app/gfx/gtk_util.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/lazy_instance.h"
#include "chrome/browser/browser_list.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "grit/generated_resources.h"
#include "grit/locale_settings.h"
#include "grit/theme_resources.h"

namespace {

// The y offset from the center at which to paint the icon.
const int kSadTabOffset = -64;
// The spacing between the icon and the title.
const int kIconTitleSpacing = 20;
// The spacing between the title and the message.
const int kTitleMessageSpacing = 15;
const int kMessageLinkSpacing = 15;
const int kTabHorzMargin = 13;
const double kBackgroundColorTop[3] = {
  35.0 / 255.0, 48.0 / 255.0, 64.0 / 255.0 };
const double kBackgroundColorBottom[3] = {
  35.0 / 255.0, 48.0 / 255.0, 64.0 / 255.0 };

struct SadTabGtkConstants {
  SadTabGtkConstants()
      : sad_tab_bitmap(
            ResourceBundle::GetSharedInstance().GetPixbufNamed(IDR_SAD_TAB)),
        title(l10n_util::GetStringUTF8(IDS_SAD_TAB_TITLE)),
        message(l10n_util::GetStringUTF8(IDS_SAD_TAB_MESSAGE)),
        learn_more_url(l10n_util::GetStringUTF8(IDS_CRASH_REASON_URL)) {}

  const GdkPixbuf* sad_tab_bitmap;
  std::string title;
  std::string message;
  std::string learn_more_url;
};

base::LazyInstance<SadTabGtkConstants>
    g_sad_tab_constants(base::LINKER_INITIALIZED);

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

SadTabGtk::SadTabGtk()
  : width_(0),
    height_(0) {
  // Use an event box to get the background painting correctly.
  event_box_.Own(gtk_event_box_new());
  gtk_widget_set_app_paintable(event_box_.get(), TRUE);
  g_signal_connect(event_box_.get(), "expose-event",
                   G_CALLBACK(OnBackgroundExposeThunk), this);
  // Listen to signal for resizing of widget.
  g_signal_connect(event_box_.get(), "size-allocate",
                   G_CALLBACK(OnSizeAllocateThunk), this);

  // Use an alignment to set the top and horizontal paddings. The padding will
  // be set later when we receive expose-event which gives us the width and
  // height of the widget.
  top_padding_ = gtk_alignment_new(0.0, 0.0, 1.0, 1.0);
  gtk_container_add(GTK_CONTAINER(event_box_.get()), top_padding_);

  // Use a vertical box to contain icon, title, message and link.
  GtkWidget* vbox = gtk_vbox_new(FALSE, 0);
  gtk_container_add(GTK_CONTAINER(top_padding_), vbox);

  const SadTabGtkConstants& sad_tab_constants = g_sad_tab_constants.Get();

  // Add center-aligned image.
  GtkWidget* image = gtk_image_new_from_pixbuf(
      const_cast<GdkPixbuf*>(sad_tab_constants.sad_tab_bitmap));
  gtk_misc_set_alignment(GTK_MISC(image), 0.5, 0.5);
  gtk_box_pack_start(GTK_BOX(vbox), image, FALSE, FALSE, kIconTitleSpacing);

  // Add center-aligned title.
  GtkWidget* title = MakeWhiteMarkupLabel(
      "<span size=\"larger\" style=\"normal\"><b>%s</b></span>",
      sad_tab_constants.title);
  gtk_box_pack_start(GTK_BOX(vbox), title, FALSE, FALSE, kTitleMessageSpacing);

  // Add center-aligned message.
  message_ = MakeWhiteMarkupLabel("<span style=\"normal\">%s</span>",
                                  sad_tab_constants.message);
  gtk_label_set_line_wrap(GTK_LABEL(message_), TRUE);
  gtk_box_pack_start(GTK_BOX(vbox), message_, FALSE, FALSE,
                     kMessageLinkSpacing);

  // Add the learn-more link and center-align it in an alignment.
  GtkWidget* link = gtk_chrome_link_button_new(
      l10n_util::GetStringUTF8(IDS_LEARN_MORE).c_str());
  g_signal_connect(link, "clicked", G_CALLBACK(OnLinkButtonClick),
                   const_cast<char*>(sad_tab_constants.learn_more_url.c_str()));
  GtkWidget* link_alignment = gtk_alignment_new(0.5, 0.5, 0.0, 0.0);
  gtk_container_add(GTK_CONTAINER(link_alignment), link);
  gtk_box_pack_start(GTK_BOX(vbox), link_alignment, FALSE, FALSE, 0);

  gtk_widget_show_all(event_box_.get());
}

SadTabGtk::~SadTabGtk() {
  event_box_.Destroy();
}

gboolean SadTabGtk::OnBackgroundExpose(
    GtkWidget* widget, GdkEventExpose* event) {
  // Clip to our damage rect.
  cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(event->window));
  cairo_rectangle(cr, event->area.x, event->area.y,
                  event->area.width, event->area.height);
  cairo_clip(cr);

  // Draw the gradient background.
  int half_width = width_ / 2;
  cairo_pattern_t* pattern = cairo_pattern_create_linear(
      half_width, 0,  half_width, height_);
  cairo_pattern_add_color_stop_rgb(
      pattern, 0.0,
      kBackgroundColorTop[0], kBackgroundColorTop[1], kBackgroundColorTop[2]);
  cairo_pattern_add_color_stop_rgb(
      pattern, 1.0,
      kBackgroundColorBottom[0], kBackgroundColorBottom[1],
      kBackgroundColorBottom[2]);
  cairo_set_source(cr, pattern);
  cairo_paint(cr);
  cairo_pattern_destroy(pattern);

  cairo_destroy(cr);

  return FALSE;  // Propagate expose event to children.
}

void SadTabGtk::OnSizeAllocate(GtkWidget* widget, GtkAllocation* allocation) {
  // Only readjust top and horizontal paddings if height of widget has changed.
  if (allocation->height != 0 && allocation->height != height_) {
    height_ = allocation->height;
    const SadTabGtkConstants& sad_tab_constants = g_sad_tab_constants.Get();
    int icon_height = gdk_pixbuf_get_height(sad_tab_constants.sad_tab_bitmap);
    int icon_y = ((height_ - icon_height) / 2) + kSadTabOffset;
    gtk_alignment_set_padding(GTK_ALIGNMENT(top_padding_), std::max(icon_y, 0),
                              0, kTabHorzMargin, kTabHorzMargin);
  }

  // Only readjust width for message if width of widget has changed.
  if (allocation->width != 0 && allocation->width != width_) {
    width_ = allocation->width;
    // Set width for wrappable message.
    gtk_widget_set_size_request(message_, width_ - (kTabHorzMargin * 2), -1);
  }
}

// static
void SadTabGtk::OnLinkButtonClick(GtkWidget* button, const char* url) {
  BrowserList::GetLastActive()->OpenURL(GURL(url), GURL(), CURRENT_TAB,
                                        PageTransition::LINK);
}
