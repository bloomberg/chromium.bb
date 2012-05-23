// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/one_click_signin_bubble_gtk.h"

#include <gtk/gtk.h>

#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Padding between content and edge of bubble (in pixels).
const int kContentBorder = 7;

}  // namespace

OneClickSigninBubbleGtk::OneClickSigninBubbleGtk(
    BrowserWindowGtk* browser_window_gtk,
    const base::Closure& learn_more_callback,
    const base::Closure& advanced_callback)
    : bubble_(NULL),
      learn_more_callback_(learn_more_callback),
      advanced_callback_(advanced_callback) {

  // Setup the BubbleGtk content.
  GtkWidget* bubble_content = gtk_vbox_new(FALSE, 0);
  gtk_container_set_border_width(GTK_CONTAINER(bubble_content),
                                 kContentBorder);

  // Message.
  GtkWidget* message_label = gtk_label_new(
      l10n_util::GetStringFUTF8(
          IDS_SYNC_PROMO_NTP_BUBBLE_MESSAGE,
          l10n_util::GetStringUTF16(IDS_SHORT_PRODUCT_NAME)).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(message_label), TRUE);
  gtk_misc_set_alignment(GTK_MISC(message_label), 0.0, 0.5);
  gtk_box_pack_start(GTK_BOX(bubble_content), message_label, FALSE, FALSE, 0);

  GtkThemeService* const theme_provider = GtkThemeService::GetFrom(
      browser_window_gtk->browser()->profile());

  GtkWidget* learn_more_line = gtk_hbox_new(FALSE, kContentBorder);
  gtk_box_pack_start(GTK_BOX(bubble_content),
                     learn_more_line, FALSE, FALSE, 0);

  // Learn more link.
  GtkWidget* learn_more_link = theme_provider->BuildChromeLinkButton(
      l10n_util::GetStringUTF8(IDS_SYNC_PROMO_NTP_BUBBLE_LEARN_MORE));
  g_signal_connect(learn_more_link, "clicked",
                   G_CALLBACK(OnClickLearnMoreLinkThunk), this);
  gtk_box_pack_start(GTK_BOX(learn_more_line),
                     learn_more_link, FALSE, FALSE, 0);

  GtkWidget* bottom_line = gtk_hbox_new(FALSE, kContentBorder);
  gtk_box_pack_start(GTK_BOX(bubble_content),
                     bottom_line, FALSE, FALSE, 0);

  // Advanced link.
  GtkWidget* advanced_link = theme_provider->BuildChromeLinkButton(
      l10n_util::GetStringUTF8(IDS_SYNC_PROMO_NTP_BUBBLE_ADVANCED));
  g_signal_connect(advanced_link, "clicked",
                   G_CALLBACK(OnClickAdvancedLinkThunk), this);
  gtk_box_pack_start(GTK_BOX(bottom_line),
                     advanced_link, FALSE, FALSE, 0);

  // OK Button.
  GtkWidget* ok_button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_OK).c_str());
  g_signal_connect(ok_button, "clicked",
                   G_CALLBACK(OnClickOKThunk), this);
  gtk_box_pack_end(GTK_BOX(bottom_line),
                   ok_button, FALSE, FALSE, 0);

  GtkWidget* const app_menu_widget =
      browser_window_gtk->GetToolbar()->GetAppMenuButton();
  gfx::Rect bounds = gtk_util::WidgetBounds(app_menu_widget);
  BubbleGtk::ArrowLocationGtk arrow_location = base::i18n::IsRTL() ?
      BubbleGtk::ARROW_LOCATION_TOP_LEFT : BubbleGtk::ARROW_LOCATION_TOP_RIGHT;
  bubble_ =
      BubbleGtk::Show(app_menu_widget, &bounds, bubble_content,
                      arrow_location,
                      BubbleGtk::MATCH_SYSTEM_THEME |
                          BubbleGtk::POPUP_WINDOW |
                          BubbleGtk::GRAB_INPUT,
                      theme_provider, this);
  gtk_widget_grab_focus(ok_button);
}

void OneClickSigninBubbleGtk::BubbleClosing(
    BubbleGtk* bubble, bool closed_by_escape) {
  delete this;
}

void OneClickSigninBubbleGtk::ClickOKForTest() {
  OnClickOK(NULL);
}

void OneClickSigninBubbleGtk::ClickLearnMoreForTest() {
  OnClickLearnMoreLink(NULL);
}

void OneClickSigninBubbleGtk::ClickAdvancedForTest() {
  OnClickAdvancedLink(NULL);
}

void OneClickSigninBubbleGtk::OnClickLearnMoreLink(GtkWidget* link) {
  learn_more_callback_.Run();
  bubble_->Close();
}

void OneClickSigninBubbleGtk::OnClickAdvancedLink(GtkWidget* link) {
  advanced_callback_.Run();
  bubble_->Close();
}

void OneClickSigninBubbleGtk::OnClickOK(GtkWidget* link) {
  bubble_->Close();
}

OneClickSigninBubbleGtk::~OneClickSigninBubbleGtk() {}
