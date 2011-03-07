// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/infobars/infobar_gtk.h"

#include <gtk/gtk.h>

#include "base/utf_string_conversions.h"
#include "chrome/browser/platform_util.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_provider.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/infobars/infobar_container_gtk.h"
#include "content/common/notification_service.h"
#include "ui/gfx/gtk_util.h"

extern const int InfoBar::kInfoBarHeight = 37;

namespace {

// Pixels between infobar elements.
const int kElementPadding = 5;

// Extra padding on either end of info bar.
const int kLeftPadding = 5;
const int kRightPadding = 5;

}  // namespace

// static
const int InfoBar::kEndOfLabelSpacing = 6;
const int InfoBar::kButtonButtonSpacing = 3;

InfoBar::InfoBar(InfoBarDelegate* delegate)
    : container_(NULL),
      delegate_(delegate),
      theme_provider_(NULL),
      arrow_model_(this) {
  // Create |hbox_| and pad the sides.
  hbox_ = gtk_hbox_new(FALSE, kElementPadding);

  // Make the whole infor bar horizontally shrinkable.
  gtk_widget_set_size_request(hbox_, 0, -1);

  GtkWidget* padding = gtk_alignment_new(0, 0, 1, 1);
  gtk_alignment_set_padding(GTK_ALIGNMENT(padding),
      0, 0, kLeftPadding, kRightPadding);

  bg_box_ = gtk_event_box_new();
  gtk_widget_set_app_paintable(bg_box_, TRUE);
  g_signal_connect(bg_box_, "expose-event",
                   G_CALLBACK(OnBackgroundExposeThunk), this);
  gtk_container_add(GTK_CONTAINER(padding), hbox_);
  gtk_container_add(GTK_CONTAINER(bg_box_), padding);
  gtk_widget_set_size_request(bg_box_, -1, kInfoBarHeight);

  // Add the icon on the left, if any.
  SkBitmap* icon = delegate->GetIcon();
  if (icon) {
    GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(icon);
    GtkWidget* image = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);
    gtk_box_pack_start(GTK_BOX(hbox_), image, FALSE, FALSE, 0);
  }

  close_button_.reset(CustomDrawButton::CloseButton(NULL));
  gtk_util::CenterWidgetInHBox(hbox_, close_button_->widget(), true, 0);
  g_signal_connect(close_button_->widget(), "clicked",
                   G_CALLBACK(OnCloseButtonThunk), this);

  slide_widget_.reset(new SlideAnimatorGtk(bg_box_,
                                           SlideAnimatorGtk::DOWN,
                                           0, true, true, this));
  // We store a pointer back to |this| so we can refer to it from the infobar
  // container.
  g_object_set_data(G_OBJECT(slide_widget_->widget()), "info-bar", this);
}

InfoBar::~InfoBar() {
}

GtkWidget* InfoBar::widget() {
  return slide_widget_->widget();
}

void InfoBar::AnimateOpen() {
  slide_widget_->Open();

  gtk_widget_show_all(bg_box_);
  if (bg_box_->window)
    gdk_window_lower(bg_box_->window);
}

void InfoBar::Open() {
  slide_widget_->OpenWithoutAnimation();

  gtk_widget_show_all(bg_box_);
  if (bg_box_->window)
    gdk_window_lower(bg_box_->window);
}

void InfoBar::AnimateClose() {
  slide_widget_->Close();
}

void InfoBar::Close() {
  if (delegate_) {
    delegate_->InfoBarClosed();
    delegate_ = NULL;
  }
  delete this;
}

bool InfoBar::IsAnimating() {
  return slide_widget_->IsAnimating();
}

bool InfoBar::IsClosing() {
  return slide_widget_->IsClosing();
}

void InfoBar::ShowArrowFor(InfoBar* other, bool animate) {
  arrow_model_.ShowArrowFor(other, animate);
}

void InfoBar::PaintStateChanged() {
  gtk_widget_queue_draw(widget());
}

void InfoBar::RemoveInfoBar() const {
  container_->RemoveDelegate(delegate_);
}

void InfoBar::Closed() {
  Close();
}

void InfoBar::SetThemeProvider(GtkThemeProvider* theme_provider) {
  if (theme_provider_) {
    NOTREACHED();
    return;
  }

  theme_provider_ = theme_provider;
  registrar_.Add(this, NotificationType::BROWSER_THEME_CHANGED,
                 NotificationService::AllSources());
  UpdateBorderColor();
}

void InfoBar::Observe(NotificationType type,
                      const NotificationSource& source,
                      const NotificationDetails& details) {
  UpdateBorderColor();
}

void InfoBar::AddLabelWithInlineLink(const string16& display_text,
                                     const string16& link_text,
                                     size_t link_offset,
                                     GCallback callback) {
  GtkWidget* link_button = gtk_chrome_link_button_new(
      UTF16ToUTF8(link_text).c_str());
  gtk_chrome_link_button_set_use_gtk_theme(
      GTK_CHROME_LINK_BUTTON(link_button), FALSE);
  gtk_util::ForceFontSizePixels(
      GTK_CHROME_LINK_BUTTON(link_button)->label, 13.4);
  DCHECK(callback);
  g_signal_connect(link_button, "clicked", callback, this);
  gtk_util::SetButtonTriggersNavigation(link_button);

  GtkWidget* hbox = gtk_hbox_new(FALSE, 0);
  // We want the link to be horizontally shrinkable, so that the Chrome
  // window can be resized freely even with a very long link.
  gtk_widget_set_size_request(hbox, 0, -1);
  gtk_box_pack_start(GTK_BOX(hbox_), hbox, TRUE, TRUE, 0);

  // Need to insert the link inside the display text.
  GtkWidget* initial_label = gtk_label_new(
      UTF16ToUTF8(display_text.substr(0, link_offset)).c_str());
  GtkWidget* trailing_label = gtk_label_new(
      UTF16ToUTF8(display_text.substr(link_offset)).c_str());

  gtk_util::ForceFontSizePixels(initial_label, 13.4);
  gtk_util::ForceFontSizePixels(trailing_label, 13.4);

  // TODO(joth): None of the label widgets are set as shrinkable here, meaning
  // the text will run under the close button etc. when the width is restricted,
  // rather than eliding.
  gtk_widget_modify_fg(initial_label, GTK_STATE_NORMAL, &gtk_util::kGdkBlack);
  gtk_widget_modify_fg(trailing_label, GTK_STATE_NORMAL, &gtk_util::kGdkBlack);

  // We don't want any spacing between the elements, so we pack them into
  // this hbox that doesn't use kElementPadding.
  gtk_box_pack_start(GTK_BOX(hbox), initial_label, FALSE, FALSE, 0);
  gtk_util::CenterWidgetInHBox(hbox, link_button, false, 0);
  gtk_box_pack_start(GTK_BOX(hbox), trailing_label, FALSE, FALSE, 0);
}

void InfoBar::GetTopColor(InfoBarDelegate::Type type,
                          double* r, double* g, double *b) {
  // These constants are copied from corresponding skia constants from
  // browser/ui/views/infobars/infobars.cc, and then changed into 0-1 ranged
  // values for cairo.
  switch (type) {
    case InfoBarDelegate::WARNING_TYPE:
      *r = 255.0 / 255.0;
      *g = 242.0 / 255.0;
      *b = 183.0 / 255.0;
      break;
    case InfoBarDelegate::PAGE_ACTION_TYPE:
      *r = 218.0 / 255.0;
      *g = 231.0 / 255.0;
      *b = 249.0 / 255.0;
      break;
  }
}

void InfoBar::GetBottomColor(InfoBarDelegate::Type type,
                             double* r, double* g, double *b) {
  switch (type) {
    case InfoBarDelegate::WARNING_TYPE:
      *r = 250.0 / 255.0;
      *g = 230.0 / 255.0;
      *b = 145.0 / 255.0;
      break;
    case InfoBarDelegate::PAGE_ACTION_TYPE:
      *r = 179.0 / 255.0;
      *g = 202.0 / 255.0;
      *b = 231.0 / 255.0;
      break;
  }
}

void InfoBar::UpdateBorderColor() {
  gtk_widget_queue_draw(widget());
}

void InfoBar::OnCloseButton(GtkWidget* button) {
  if (delegate_)
    delegate_->InfoBarDismissed();
  RemoveInfoBar();
}

gboolean InfoBar::OnBackgroundExpose(GtkWidget* sender,
                                     GdkEventExpose* event) {
  const int height = sender->allocation.height;

  cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(sender->window));
  gdk_cairo_rectangle(cr, &event->area);
  cairo_clip(cr);

  cairo_pattern_t* pattern = cairo_pattern_create_linear(0, 0, 0, height);

  double top_r, top_g, top_b;
  GetTopColor(delegate_->GetInfoBarType(), &top_r, &top_g, &top_b);
  cairo_pattern_add_color_stop_rgb(pattern, 0.0, top_r, top_g, top_b);

  double bottom_r, bottom_g, bottom_b;
  GetBottomColor(delegate_->GetInfoBarType(), &bottom_r, &bottom_g, &bottom_b);
  cairo_pattern_add_color_stop_rgb(
      pattern, 1.0, bottom_r, bottom_g, bottom_b);
  cairo_set_source(cr, pattern);
  cairo_paint(cr);
  cairo_pattern_destroy(pattern);

  // Draw the bottom border.
  GdkColor border_color = theme_provider_->GetBorderColor();
  cairo_set_source_rgb(cr, border_color.red / 65535.0,
                           border_color.green / 65535.0,
                           border_color.blue / 65535.0);
  cairo_set_line_width(cr, 1.0);
  int y = sender->allocation.height;
  cairo_move_to(cr, 0, y - 0.5);
  cairo_rel_line_to(cr, sender->allocation.width, 0);
  cairo_stroke(cr);

  cairo_destroy(cr);

  if (!arrow_model_.NeedToDrawInfoBarArrow())
    return FALSE;

  GtkWindow* parent = platform_util::GetTopLevel(widget());
  BrowserWindowGtk* browser_window =
      BrowserWindowGtk::GetBrowserWindowForNativeWindow(parent);
  int x = browser_window ?
      browser_window->GetXPositionOfLocationIcon(sender) : 0;

  arrow_model_.Paint(sender, event, gfx::Point(x, y), border_color);

  return FALSE;
}
