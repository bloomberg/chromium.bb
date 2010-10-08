// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/infobar_gtk.h"

#include <gtk/gtk.h>

#include "base/utf_string_conversions.h"
#include "chrome/browser/gtk/custom_button.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/gtk/gtk_chrome_shrinkable_hbox.h"
#include "chrome/browser/gtk/gtk_theme_provider.h"
#include "chrome/browser/gtk/gtk_util.h"
#include "chrome/browser/gtk/infobar_container_gtk.h"
#include "chrome/common/notification_service.h"
#include "gfx/gtk_util.h"

extern const int InfoBar::kInfoBarHeight = 37;

namespace {

// Spacing after message (and before buttons).
const int kEndOfLabelSpacing = 6;
// Spacing between buttons.
const int kButtonButtonSpacing = 3;

// Pixels between infobar elements.
const int kElementPadding = 5;

// Extra padding on either end of info bar.
const int kLeftPadding = 5;
const int kRightPadding = 5;

}  // namespace

InfoBar::InfoBar(InfoBarDelegate* delegate)
    : container_(NULL),
      delegate_(delegate),
      theme_provider_(NULL) {
  // Create |hbox_| and pad the sides.
  hbox_ = gtk_hbox_new(FALSE, kElementPadding);

  // Make the whole infor bar horizontally shrinkable.
  gtk_widget_set_size_request(hbox_, 0, -1);

  GtkWidget* padding = gtk_alignment_new(0, 0, 1, 1);
  gtk_alignment_set_padding(GTK_ALIGNMENT(padding),
      0, 0, kLeftPadding, kRightPadding);

  GtkWidget* bg_box = gtk_event_box_new();
  gtk_widget_set_app_paintable(bg_box, TRUE);
  g_signal_connect(bg_box, "expose-event",
                   G_CALLBACK(OnBackgroundExposeThunk), this);
  gtk_container_add(GTK_CONTAINER(padding), hbox_);
  gtk_container_add(GTK_CONTAINER(bg_box), padding);
  // The -1 on the kInfoBarHeight is to account for the border.
  gtk_widget_set_size_request(bg_box, -1, kInfoBarHeight - 1);

  border_bin_.Own(gtk_util::CreateGtkBorderBin(bg_box, NULL,
                                               0, 1, 0, 0));

  // Add the icon on the left, if any.
  SkBitmap* icon = delegate->GetIcon();
  if (icon) {
    GdkPixbuf* pixbuf = gfx::GdkPixbufFromSkBitmap(icon);
    GtkWidget* image = gtk_image_new_from_pixbuf(pixbuf);
    g_object_unref(pixbuf);
    gtk_box_pack_start(GTK_BOX(hbox_), image, FALSE, FALSE, 0);
  }

  // TODO(erg): GTK theme the info bar.
  close_button_.reset(CustomDrawButton::CloseButton(NULL));
  gtk_util::CenterWidgetInHBox(hbox_, close_button_->widget(), true, 0);
  g_signal_connect(close_button_->widget(), "clicked",
                   G_CALLBACK(OnCloseButtonThunk), this);

  slide_widget_.reset(new SlideAnimatorGtk(border_bin_.get(),
                                           SlideAnimatorGtk::DOWN,
                                           0, true, true, this));
  // We store a pointer back to |this| so we can refer to it from the infobar
  // container.
  g_object_set_data(G_OBJECT(slide_widget_->widget()), "info-bar", this);
}

InfoBar::~InfoBar() {
  border_bin_.Destroy();
}

GtkWidget* InfoBar::widget() {
  return slide_widget_->widget();
}

void InfoBar::AnimateOpen() {
  slide_widget_->Open();
  if (border_bin_->window)
    gdk_window_lower(border_bin_->window);
}

void InfoBar::Open() {
  slide_widget_->OpenWithoutAnimation();
  if (border_bin_->window)
    gdk_window_lower(border_bin_->window);
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

  // TODO(joth): Unlike the AddLabalAndLink below, none of the label widgets
  // are set as shrinkable here, meaning the text will run under the close
  // button etc. when the width is restricted, rather than eliding.
  gtk_widget_modify_fg(initial_label, GTK_STATE_NORMAL, &gfx::kGdkBlack);
  gtk_widget_modify_fg(trailing_label, GTK_STATE_NORMAL, &gfx::kGdkBlack);

  // We don't want any spacing between the elements, so we pack them into
  // this hbox that doesn't use kElementPadding.
  gtk_box_pack_start(GTK_BOX(hbox), initial_label, FALSE, FALSE, 0);
  gtk_util::CenterWidgetInHBox(hbox, link_button, false, 0);
  gtk_box_pack_start(GTK_BOX(hbox), trailing_label, FALSE, FALSE, 0);
}

// TODO(joth): This method factors out some common functionality between the
// various derived infobar classes, however the class hierarchy itself could
// use refactoring to reduce this duplication. http://crbug.com/38924
void InfoBar::AddLabelAndLink(const string16& display_text,
                              const string16& link_text,
                              GCallback callback) {
  GtkWidget* link_button = NULL;
  if (!link_text.empty()) {
    // If we have some link text, create the link button.
    link_button = gtk_chrome_link_button_new(UTF16ToUTF8(link_text).c_str());
    gtk_chrome_link_button_set_use_gtk_theme(
        GTK_CHROME_LINK_BUTTON(link_button), FALSE);
    DCHECK(callback);
    g_signal_connect(link_button, "clicked", callback, this);
    gtk_util::SetButtonTriggersNavigation(link_button);
  }

  GtkWidget* hbox = gtk_hbox_new(FALSE, 0);
  // We want the link to be horizontally shrinkable, so that the Chrome
  // window can be resized freely even with a very long link.
  gtk_widget_set_size_request(hbox, 0, -1);
  gtk_box_pack_start(GTK_BOX(hbox_), hbox, TRUE, TRUE, 0);

  if (link_button)
    gtk_box_pack_end(GTK_BOX(hbox), link_button, FALSE, FALSE, 0);
  GtkWidget* label = gtk_label_new(UTF16ToUTF8(display_text).c_str());
  // In order to avoid the link_button and the label overlapping with each
  // other, we make the label shrinkable.
  gtk_widget_set_size_request(label, 0, -1);
  gtk_label_set_ellipsize(GTK_LABEL(label), PANGO_ELLIPSIZE_END);
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
  gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &gfx::kGdkBlack);
  gtk_box_pack_start(GTK_BOX(hbox), label, TRUE, TRUE, 0);
}

void InfoBar::GetTopColor(InfoBarDelegate::Type type,
                          double* r, double* g, double *b) {
  // These constants are copied from corresponding skia constants from
  // browser/views/infobars/infobars.cc, and then changed into 0-1 ranged
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
  GdkColor border_color = theme_provider_->GetBorderColor();
  gtk_widget_modify_bg(border_bin_.get(), GTK_STATE_NORMAL, &border_color);
}

void InfoBar::OnCloseButton(GtkWidget* button) {
  if (delegate_)
    delegate_->InfoBarDismissed();
  RemoveInfoBar();
}

gboolean InfoBar::OnBackgroundExpose(GtkWidget* widget,
                                     GdkEventExpose* event) {
  const int height = widget->allocation.height;

  cairo_t* cr = gdk_cairo_create(GDK_DRAWABLE(widget->window));
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

  cairo_destroy(cr);

  return FALSE;
}

// AlertInfoBar ----------------------------------------------------------------

class AlertInfoBar : public InfoBar {
 public:
  explicit AlertInfoBar(AlertInfoBarDelegate* delegate)
      : InfoBar(delegate) {
    AddLabelAndLink(delegate->GetMessageText(), string16(), NULL);
    gtk_widget_show_all(border_bin_.get());
  }
};

// LinkInfoBar -----------------------------------------------------------------

class LinkInfoBar : public InfoBar {
 public:
  explicit LinkInfoBar(LinkInfoBarDelegate* delegate)
      : InfoBar(delegate) {
    size_t link_offset;
    string16 display_text = delegate->GetMessageTextWithOffset(&link_offset);
    string16 link_text = delegate->GetLinkText();
    AddLabelWithInlineLink(display_text, link_text, link_offset,
                           G_CALLBACK(OnLinkClick));
    gtk_widget_show_all(border_bin_.get());
  }

 private:
  static void OnLinkClick(GtkWidget* button, LinkInfoBar* link_info_bar) {
    if (link_info_bar->delegate_->AsLinkInfoBarDelegate()->
        LinkClicked(gtk_util::DispositionForCurrentButtonPressEvent())) {
      link_info_bar->RemoveInfoBar();
    }
  }
};

// ConfirmInfoBar --------------------------------------------------------------

class ConfirmInfoBar : public InfoBar {
 public:
  explicit ConfirmInfoBar(ConfirmInfoBarDelegate* delegate);

 private:
  // Adds a button to the info bar by type. It will do nothing if the delegate
  // doesn't specify a button of the given type.
  void AddButton(ConfirmInfoBarDelegate::InfoBarButton type);

  CHROMEGTK_CALLBACK_0(ConfirmInfoBar, void, OnOkButton);
  CHROMEGTK_CALLBACK_0(ConfirmInfoBar, void, OnCancelButton);
  CHROMEGTK_CALLBACK_0(ConfirmInfoBar, void, OnLinkClicked);

  GtkWidget* confirm_hbox_;

  DISALLOW_COPY_AND_ASSIGN(ConfirmInfoBar);
};

ConfirmInfoBar::ConfirmInfoBar(ConfirmInfoBarDelegate* delegate)
    : InfoBar(delegate) {
  confirm_hbox_ = gtk_chrome_shrinkable_hbox_new(FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(hbox_), confirm_hbox_, TRUE, TRUE, 0);
  gtk_widget_set_size_request(confirm_hbox_, 0, -1);

  std::string label_text = UTF16ToUTF8(delegate->GetMessageText());
  GtkWidget* label = gtk_label_new(label_text.c_str());
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
  gtk_util::CenterWidgetInHBox(confirm_hbox_, label, false, kEndOfLabelSpacing);
  gtk_widget_modify_fg(label, GTK_STATE_NORMAL, &gfx::kGdkBlack);
  g_signal_connect(label, "map",
                   G_CALLBACK(gtk_util::InitLabelSizeRequestAndEllipsizeMode),
                   NULL);

  AddButton(ConfirmInfoBarDelegate::BUTTON_CANCEL);
  AddButton(ConfirmInfoBarDelegate::BUTTON_OK);

  std::string link_text = UTF16ToUTF8(delegate->GetLinkText());
  GtkWidget* link = gtk_chrome_link_button_new(link_text.c_str());
  gtk_misc_set_alignment(GTK_MISC(GTK_CHROME_LINK_BUTTON(link)->label), 0, 0.5);
  g_signal_connect(link, "clicked", G_CALLBACK(OnLinkClickedThunk), this);
  gtk_util::SetButtonTriggersNavigation(link);
  // Until we switch to vector graphics, force the font size.
  // 13.4px == 10pt @ 96dpi
  gtk_util::ForceFontSizePixels(GTK_CHROME_LINK_BUTTON(link)->label, 13.4);
  gtk_util::CenterWidgetInHBox(hbox_, link, true, kEndOfLabelSpacing);

  gtk_widget_show_all(border_bin_.get());
}

void ConfirmInfoBar::AddButton(ConfirmInfoBarDelegate::InfoBarButton type) {
  if (delegate_->AsConfirmInfoBarDelegate()->GetButtons() & type) {
    GtkWidget* button = gtk_button_new_with_label(UTF16ToUTF8(
        delegate_->AsConfirmInfoBarDelegate()->GetButtonLabel(type)).c_str());
    gtk_util::CenterWidgetInHBox(confirm_hbox_, button, false,
                                 kButtonButtonSpacing);
    g_signal_connect(button, "clicked",
                     G_CALLBACK(type == ConfirmInfoBarDelegate::BUTTON_OK ?
                                OnOkButtonThunk : OnCancelButtonThunk),
                     this);
  }
}

void ConfirmInfoBar::OnCancelButton(GtkWidget* widget) {
  if (delegate_->AsConfirmInfoBarDelegate()->Cancel())
    RemoveInfoBar();
}

void ConfirmInfoBar::OnOkButton(GtkWidget* widget) {
  if (delegate_->AsConfirmInfoBarDelegate()->Accept())
    RemoveInfoBar();
}

void ConfirmInfoBar::OnLinkClicked(GtkWidget* widget) {
  if (delegate_->AsConfirmInfoBarDelegate()->LinkClicked(
          gtk_util::DispositionForCurrentButtonPressEvent())) {
    RemoveInfoBar();
  }
}

InfoBar* AlertInfoBarDelegate::CreateInfoBar() {
  return new AlertInfoBar(this);
}
InfoBar* LinkInfoBarDelegate::CreateInfoBar() {
  return new LinkInfoBar(this);
}
InfoBar* ConfirmInfoBarDelegate::CreateInfoBar() {
  return new ConfirmInfoBar(this);
}
