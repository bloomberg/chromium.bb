// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/manage_passwords_bubble_gtk.h"

#include "chrome/browser/chrome_notification_types.h"
#include "chrome/browser/content_settings/tab_specific_content_settings.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/location_bar_view_gtk.h"
#include "chrome/browser/ui/passwords/manage_passwords_icon_controller.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Pointer to singleton object (NULL if no bubble is open).
ManagePasswordsBubbleGtk* g_bubble = NULL;

// Need to manually set anchor width and height to ensure that the bubble shows
// in the correct spot the first time it is displayed when no icon is present.
const int kBubbleAnchorWidth = 20;
const int kBubbleAnchorHeight = 25;

}  // namespace

// static
void ManagePasswordsBubbleGtk::ShowBubble(content::WebContents* web_contents) {
  Browser* browser = chrome::FindBrowserWithWebContents(web_contents);
  DCHECK(browser && browser->window() && browser->fullscreen_controller());

  LocationBar* location_bar = browser->window()->GetLocationBar();
  GtkWidget* anchor = browser->window()->IsFullscreen() ?
      GTK_WIDGET(browser->window()->GetNativeWindow()) :
      static_cast<LocationBarViewGtk*>(location_bar)->
          manage_passwords_icon_widget();

  g_bubble = new ManagePasswordsBubbleGtk(anchor,
                                          web_contents,
                                          browser->fullscreen_controller());
}

// static
void ManagePasswordsBubbleGtk::CloseBubble() {
  if (g_bubble)
    g_bubble->Close();
}

// static
bool ManagePasswordsBubbleGtk::IsShowing() {
  return g_bubble != NULL;
}

ManagePasswordsBubbleGtk::ManagePasswordsBubbleGtk(
    GtkWidget* anchor,
    content::WebContents* web_contents,
    FullscreenController* fullscreen_controller)
    : web_contents_(web_contents) {
  GtkThemeService* theme_service = GtkThemeService::GetFrom(
      Profile::FromBrowserContext(web_contents_->GetBrowserContext()));

  GtkWidget* bubble_contents_ = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(bubble_contents_),
                                 ui::kContentAreaBorder);
  GtkWidget* label = theme_service->BuildLabel(
      l10n_util::GetStringUTF8(IDS_SAVE_PASSWORD), ui::kGdkBlack);
  gtk_misc_set_alignment(GTK_MISC(label), 0, 0.5);
  gtk_box_pack_start(GTK_BOX(bubble_contents_), label, FALSE, FALSE, 0);

  GtkWidget* button_container = gtk_hbox_new(FALSE, 0);
  GtkWidget* nope_button = gtk_button_new_with_label(l10n_util::GetStringUTF8(
      IDS_PASSWORD_MANAGER_CANCEL_BUTTON).c_str());
  g_signal_connect(nope_button, "clicked",
                   G_CALLBACK(OnNotNowButtonClickedThunk), this);
  GtkWidget* save_button = gtk_button_new_with_label(
      l10n_util::GetStringUTF8(IDS_PASSWORD_MANAGER_SAVE_BUTTON).c_str());
  g_signal_connect(save_button, "clicked",
                   G_CALLBACK(OnSaveButtonClickedThunk), this);

  gtk_box_pack_end(GTK_BOX(button_container), save_button, FALSE, FALSE, 4);
  gtk_box_pack_end(GTK_BOX(button_container), nope_button, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(bubble_contents_), button_container, FALSE, FALSE,
                     0);
  gtk_widget_grab_focus(save_button);

  gfx::Rect rect = gfx::Rect(kBubbleAnchorWidth, kBubbleAnchorHeight);
  BubbleGtk::FrameStyle frame_style = gtk_widget_is_toplevel(anchor) ?
      BubbleGtk::FIXED_TOP_RIGHT : BubbleGtk::ANCHOR_TOP_RIGHT;
  bubble_ = BubbleGtk::Show(anchor,
                            &rect,
                            bubble_contents_,
                            frame_style,
                            BubbleGtk::MATCH_SYSTEM_THEME |
                                BubbleGtk::POPUP_WINDOW |
                                BubbleGtk::GRAB_INPUT,
                            theme_service,
                            NULL);

  g_signal_connect(bubble_contents_, "destroy",
                   G_CALLBACK(&OnDestroyThunk), this);
}

ManagePasswordsBubbleGtk::~ManagePasswordsBubbleGtk() {
  DCHECK_EQ(g_bubble, this);
  // Set singleton pointer to NULL.
  g_bubble = NULL;
}

void ManagePasswordsBubbleGtk::Close() {
  DCHECK(bubble_);
  bubble_->Close();
}

void ManagePasswordsBubbleGtk::OnDestroy(GtkWidget* widget) {
  // Listen to the destroy signal and delete this instance when it is caught.
  delete this;
}

void ManagePasswordsBubbleGtk::OnSaveButtonClicked(GtkWidget* button) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents_);
  content_settings->set_password_action(PasswordFormManager::SAVE);
  Close();
}

void ManagePasswordsBubbleGtk::OnNotNowButtonClicked(GtkWidget* button) {
  TabSpecificContentSettings* content_settings =
      TabSpecificContentSettings::FromWebContents(web_contents_);
  content_settings->set_password_action(PasswordFormManager::DO_NOTHING);
  Close();
}
