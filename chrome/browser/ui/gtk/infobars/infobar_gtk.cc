// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/infobars/infobar_gtk.h"

#include "base/debug/trace_event.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/infobars/infobar_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/themes/theme_properties.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/infobars/infobar_container_gtk.h"
#include "chrome/common/chrome_notification_types.h"
#include "content/public/browser/notification_source.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/gtk/gtk_expanded_container.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/gtk/gtk_signal_registrar.h"
#include "ui/base/models/menu_model.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image/image.h"

namespace {

// Pixels between infobar elements.
const int kElementPadding = 5;

// Extra padding on either end of info bar.
const int kLeftPadding = 5;
const int kRightPadding = 5;

// Spacing between buttons.
const int kButtonButtonSpacing = 3;

}  // namespace

// static
const int InfoBar::kSeparatorLineHeight = 1;
const int InfoBar::kDefaultArrowTargetHeight = 9;
const int InfoBar::kMaximumArrowTargetHeight = 24;
const int InfoBar::kDefaultArrowTargetHalfWidth = kDefaultArrowTargetHeight;
const int InfoBar::kMaximumArrowTargetHalfWidth = 14;
const int InfoBar::kDefaultBarTargetHeight = 36;

// static
const int InfoBarGtk::kEndOfLabelSpacing = 6;

InfoBarGtk::InfoBarGtk(InfoBarService* owner, InfoBarDelegate* delegate)
    : InfoBar(owner, delegate),
      theme_service_(GtkThemeService::GetFrom(Profile::FromBrowserContext(
          owner->GetWebContents()->GetBrowserContext()))),
      signals_(new ui::GtkSignalRegistrar) {
  DCHECK(delegate);
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

  // Add the icon on the left, if any.
  gfx::Image* icon = delegate->GetIcon();
  if (icon) {
    GtkWidget* image = gtk_image_new_from_pixbuf(icon->ToGdkPixbuf());

    gtk_misc_set_alignment(GTK_MISC(image), 0.5, 0.5);

    gtk_box_pack_start(GTK_BOX(hbox_), image, FALSE, FALSE, 0);
  }

  close_button_.reset(CustomDrawButton::CloseButton(theme_service_));
  gtk_util::CenterWidgetInHBox(hbox_, close_button_->widget(), true, 0);
  signals_->Connect(close_button_->widget(), "clicked",
                    G_CALLBACK(OnCloseButtonThunk), this);

  widget_.Own(gtk_expanded_container_new());
  gtk_container_add(GTK_CONTAINER(widget_.get()), bg_box_);
  gtk_widget_set_size_request(widget_.get(), -1, 0);

  g_signal_connect(widget_.get(), "child-size-request",
                   G_CALLBACK(OnChildSizeRequestThunk),
                   this);

  registrar_.Add(this, chrome::NOTIFICATION_BROWSER_THEME_CHANGED,
                 content::Source<ThemeService>(theme_service_));
  UpdateBorderColor();
}

InfoBarGtk::~InfoBarGtk() {
}

GtkWidget* InfoBarGtk::widget() {
  return widget_.get();
}

GdkColor InfoBarGtk::GetBorderColor() const {
  return theme_service_->GetBorderColor();
}

int InfoBarGtk::AnimatingHeight() const {
  return animation().is_animating() ? bar_target_height() : 0;
}

ui::GtkSignalRegistrar* InfoBarGtk::Signals() {
  return signals_.get();
}

GtkWidget* InfoBarGtk::CreateLabel(const std::string& text) {
  return theme_service_->BuildLabel(text, ui::kGdkBlack);
}

GtkWidget* InfoBarGtk::CreateLinkButton(const std::string& text) {
  return theme_service_->BuildChromeLinkButton(text);
}

// static
GtkWidget* InfoBarGtk::CreateMenuButton(const std::string& text) {
  GtkWidget* button = gtk_button_new();
  GtkWidget* former_child = gtk_bin_get_child(GTK_BIN(button));
  if (former_child)
    gtk_container_remove(GTK_CONTAINER(button), former_child);

  GtkWidget* hbox = gtk_hbox_new(FALSE, 0);

  GtkWidget* label = gtk_label_new(text.c_str());
  gtk_box_pack_start(GTK_BOX(hbox), label, FALSE, FALSE, 0);

  GtkWidget* arrow = gtk_arrow_new(GTK_ARROW_DOWN, GTK_SHADOW_NONE);
  gtk_box_pack_start(GTK_BOX(hbox), arrow, FALSE, FALSE, 0);

  gtk_container_add(GTK_CONTAINER(button), hbox);

  return button;
}

SkColor InfoBarGtk::ConvertGetColor(ColorGetter getter) {
  double r, g, b;
  (this->*getter)(delegate()->GetInfoBarType(), &r, &g, &b);
  return SkColorSetARGB(255, 255 * r, 255 * g, 255 * b);
}

void InfoBarGtk::AddLabelWithInlineLink(const string16& display_text,
                                        const string16& link_text,
                                        size_t link_offset,
                                        GCallback callback) {
  GtkWidget* link_button = CreateLinkButton(UTF16ToUTF8(link_text));
  gtk_util::ForceFontSizePixels(
      GTK_CHROME_LINK_BUTTON(link_button)->label, 13.4);
  DCHECK(callback);
  signals_->Connect(link_button, "clicked", callback, this);
  gtk_util::SetButtonTriggersNavigation(link_button);

  GtkWidget* hbox = gtk_hbox_new(FALSE, 0);
  // We want the link to be horizontally shrinkable, so that the Chrome
  // window can be resized freely even with a very long link.
  gtk_widget_set_size_request(hbox, 0, -1);
  gtk_box_pack_start(GTK_BOX(hbox_), hbox, TRUE, TRUE, 0);

  // Need to insert the link inside the display text.
  GtkWidget* initial_label = CreateLabel(
      UTF16ToUTF8(display_text.substr(0, link_offset)));
  GtkWidget* trailing_label = CreateLabel(
      UTF16ToUTF8(display_text.substr(link_offset)));

  gtk_util::ForceFontSizePixels(initial_label, 13.4);
  gtk_util::ForceFontSizePixels(trailing_label, 13.4);

  // TODO(joth): None of the label widgets are set as shrinkable here, meaning
  // the text will run under the close button etc. when the width is restricted,
  // rather than eliding.

  // We don't want any spacing between the elements, so we pack them into
  // this hbox that doesn't use kElementPadding.
  gtk_box_pack_start(GTK_BOX(hbox), initial_label, FALSE, FALSE, 0);
  gtk_util::CenterWidgetInHBox(hbox, link_button, false, 0);
  gtk_box_pack_start(GTK_BOX(hbox), trailing_label, FALSE, FALSE, 0);
}

void InfoBarGtk::ShowMenuWithModel(GtkWidget* sender,
                                   MenuGtk::Delegate* delegate,
                                   ui::MenuModel* model) {
  menu_.reset(new MenuGtk(delegate, model));
  menu_->PopupForWidget(sender, 1, gtk_get_current_event_time());
}

void InfoBarGtk::GetTopColor(InfoBarDelegate::Type type,
                             double* r, double* g, double* b) {
  SkColor color = theme_service_->UsingNativeTheme() ?
                  theme_service_->GetColor(ThemeProperties::COLOR_TOOLBAR) :
                  GetInfoBarTopColor(type);
  *r = SkColorGetR(color) / 255.0;
  *g = SkColorGetG(color) / 255.0;
  *b = SkColorGetB(color) / 255.0;
}

void InfoBarGtk::GetBottomColor(InfoBarDelegate::Type type,
                                double* r, double* g, double* b) {
  SkColor color = theme_service_->UsingNativeTheme() ?
                  theme_service_->GetColor(ThemeProperties::COLOR_TOOLBAR) :
                  GetInfoBarBottomColor(type);
  *r = SkColorGetR(color) / 255.0;
  *g = SkColorGetG(color) / 255.0;
  *b = SkColorGetB(color) / 255.0;
}

void InfoBarGtk::UpdateBorderColor() {
  gtk_widget_queue_draw(widget());
}

void InfoBarGtk::OnCloseButton(GtkWidget* button) {
  // If we're not owned, we're already closing, so don't call
  // InfoBarDismissed(), since this can lead to us double-recording dismissals.
  if (delegate() && owned())
    delegate()->InfoBarDismissed();
  RemoveSelf();
}

gboolean InfoBarGtk::OnBackgroundExpose(GtkWidget* sender,
                                        GdkEventExpose* event) {
  TRACE_EVENT0("ui::gtk", "InfoBarGtk::OnBackgroundExpose");

  GtkAllocation allocation;
  gtk_widget_get_allocation(sender, &allocation);
  const int height = allocation.height;

  cairo_t* cr = gdk_cairo_create(gtk_widget_get_window(sender));
  gdk_cairo_rectangle(cr, &event->area);
  cairo_clip(cr);

  cairo_pattern_t* pattern = cairo_pattern_create_linear(0, 0, 0, height);

  double top_r, top_g, top_b;
  GetTopColor(delegate()->GetInfoBarType(), &top_r, &top_g, &top_b);
  cairo_pattern_add_color_stop_rgb(pattern, 0.0, top_r, top_g, top_b);

  double bottom_r, bottom_g, bottom_b;
  GetBottomColor(delegate()->GetInfoBarType(), &bottom_r, &bottom_g, &bottom_b);
  cairo_pattern_add_color_stop_rgb(
      pattern, 1.0, bottom_r, bottom_g, bottom_b);
  cairo_set_source(cr, pattern);
  cairo_paint(cr);
  cairo_pattern_destroy(pattern);

  // Draw the bottom border.
  GdkColor border_color = GetBorderColor();
  cairo_set_source_rgb(cr, border_color.red / 65535.0,
                       border_color.green / 65535.0,
                       border_color.blue / 65535.0);
  cairo_set_line_width(cr, 1.0);
  cairo_move_to(cr, 0, allocation.height - 0.5);
  cairo_rel_line_to(cr, allocation.width, 0);
  cairo_stroke(cr);

  cairo_destroy(cr);

  if (container()) {
    static_cast<InfoBarContainerGtk*>(container())->
        PaintInfobarBitsOn(sender, event, this);
  }

  return FALSE;
}

void InfoBarGtk::PlatformSpecificShow(bool animate) {
  gtk_widget_show_all(widget_.get());
  gtk_widget_set_size_request(widget_.get(), -1, bar_height());

  GdkWindow* gdk_window = gtk_widget_get_window(bg_box_);
  if (gdk_window)
    gdk_window_lower(gdk_window);
}

void InfoBarGtk::PlatformSpecificOnCloseSoon() {
  // We must close all menus and prevent any signals from being emitted while
  // we are animating the info bar closed.
  menu_.reset();
  signals_.reset();
}

void InfoBarGtk::PlatformSpecificOnHeightsRecalculated() {
  gtk_widget_set_size_request(bg_box_, -1, bar_target_height());
  gtk_expanded_container_move(GTK_EXPANDED_CONTAINER(widget_.get()),
                              bg_box_, 0,
                              bar_height() - bar_target_height());

  gtk_widget_set_size_request(widget_.get(), -1, bar_height());
  gtk_widget_queue_draw(widget_.get());
}

void InfoBarGtk::Observe(int type,
                         const content::NotificationSource& source,
                         const content::NotificationDetails& details) {
  UpdateBorderColor();
}

void InfoBarGtk::OnChildSizeRequest(GtkWidget* expanded,
                                    GtkWidget* child,
                                    GtkRequisition* requisition) {
  requisition->height = -1;
}
