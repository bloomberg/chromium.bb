// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/one_click_signin_bubble_gtk.h"

#include <gtk/gtk.h>

#include "base/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/tabs/tab_strip_gtk.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"

static const int kModalDialogMessageWidth = 400;

OneClickSigninBubbleGtk::OneClickSigninBubbleGtk(
    BrowserWindowGtk* browser_window_gtk,
    BrowserWindow::OneClickSigninBubbleType type,
    const BrowserWindow::StartSyncCallback& start_sync_callback)
    : bubble_(NULL),
      start_sync_callback_(start_sync_callback),
      is_modal_(type != BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE),
      message_label_(NULL),
      advanced_link_(NULL),
      ok_button_(NULL),
      undo_button_(NULL),
      learn_more_(NULL),
      header_label_(NULL) {
  InitializeWidgets(browser_window_gtk);
  ShowWidget(browser_window_gtk, LayoutWidgets());
}

void OneClickSigninBubbleGtk::BubbleClosing(
    BubbleGtk* bubble, bool closed_by_escape) {
  if (!start_sync_callback_.is_null()) {
    base::ResetAndReturn(&start_sync_callback_).Run(
        OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS);
  }

  // The bubble needs to close and remove the widgets from the window before
  // |close_button_| (which is a CustomDrawButton) can be destroyed, because it
  // depends on all references being cleared for the GtkWidget before it is
  // destroyed.
  MessageLoopForUI::current()->DeleteSoon(FROM_HERE, close_button_.release());

  delete this;
}

void OneClickSigninBubbleGtk::OnClickAdvancedLink(GtkWidget* link) {
  base::ResetAndReturn(&start_sync_callback_).Run(
      OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST);
  bubble_->Close();
}

void OneClickSigninBubbleGtk::OnClickOK(GtkWidget* link) {
  base::ResetAndReturn(&start_sync_callback_).Run(
      OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS);
  bubble_->Close();
}

void OneClickSigninBubbleGtk::OnClickUndo(GtkWidget* link) {
  base::ResetAndReturn(&start_sync_callback_).Run(
      OneClickSigninSyncStarter::UNDO_SYNC);
  bubble_->Close();
}

void OneClickSigninBubbleGtk::OnClickLearnMoreLink(GtkWidget* link) {
  Browser* browser = chrome::FindBrowserWithWindow(
      gtk_window_get_transient_for(bubble_->GetNativeWindow()));
  DCHECK(browser);
  chrome::NavigateParams params(browser, GURL(chrome::kChromeSyncLearnMoreURL),
      content::PAGE_TRANSITION_LINK);
  params.disposition = NEW_WINDOW;
  chrome::Navigate(&params);
}

void OneClickSigninBubbleGtk::OnClickCloseButton(GtkWidget* button) {
  start_sync_callback_.Reset();
  bubble_->Close();
}

OneClickSigninBubbleGtk::~OneClickSigninBubbleGtk() {
}

void OneClickSigninBubbleGtk::InitializeWidgets(
    BrowserWindowGtk* browser_window_gtk) {
  // Message.
  message_label_ = gtk_label_new(l10n_util::GetStringUTF8(
      is_modal_ ? IDS_ONE_CLICK_SIGNIN_DIALOG_MESSAGE :
                  IDS_ONE_CLICK_SIGNIN_BUBBLE_MESSAGE).c_str());
  gtk_label_set_line_wrap(GTK_LABEL(message_label_), TRUE);
  gtk_misc_set_alignment(GTK_MISC(message_label_), 0.0, 0.5);
  if (is_modal_)
    gtk_widget_set_size_request(message_label_, kModalDialogMessageWidth, -1);

  GtkThemeService* const theme_provider = GtkThemeService::GetFrom(
      browser_window_gtk->browser()->profile());

  // Advanced link.
  advanced_link_ = theme_provider->BuildChromeLinkButton(
      l10n_util::GetStringUTF8(
          is_modal_ ? IDS_ONE_CLICK_SIGNIN_DIALOG_ADVANCED :
                      IDS_SYNC_PROMO_NTP_BUBBLE_ADVANCED));
  g_signal_connect(advanced_link_, "clicked",
                   G_CALLBACK(OnClickAdvancedLinkThunk), this);

  // Make the OK and Undo buttons the same size horizontally.
  GtkSizeGroup* size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
  // OK Button.
  ok_button_ = gtk_button_new_with_label(l10n_util::GetStringUTF8(
      is_modal_ ? IDS_ONE_CLICK_SIGNIN_DIALOG_OK_BUTTON : IDS_OK).c_str());
  g_signal_connect(ok_button_, "clicked",
                   G_CALLBACK(OnClickOKThunk), this);
  gtk_size_group_add_widget(size_group, ok_button_);

  // Undo Button.
  undo_button_ = gtk_button_new_with_label(l10n_util::GetStringUTF8(
        is_modal_? IDS_ONE_CLICK_SIGNIN_DIALOG_UNDO_BUTTON :
                   IDS_ONE_CLICK_BUBBLE_UNDO).c_str());
  g_signal_connect(undo_button_, "clicked",
                   G_CALLBACK(OnClickUndoThunk), this);
  gtk_size_group_add_widget(size_group, undo_button_);

  g_object_unref(size_group);

  if (!is_modal_)
    return;

  // The close button and the 'Learn More...' link are only in the modal dialog.
  learn_more_ = theme_provider->BuildChromeLinkButton(
      l10n_util::GetStringUTF8(IDS_LEARN_MORE));
  g_signal_connect(learn_more_, "clicked",
                   G_CALLBACK(OnClickLearnMoreLinkThunk), this);

  header_label_ = theme_provider->BuildLabel(
      l10n_util::GetStringUTF8(IDS_ONE_CLICK_SIGNIN_DIALOG_TITLE),
      ui::kGdkBlack);

  PangoAttrList* attributes = pango_attr_list_new();
  pango_attr_list_insert(attributes,
                         pango_attr_weight_new(PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes(GTK_LABEL(header_label_), attributes);
  pango_attr_list_unref(attributes);
  close_button_.reset(CustomDrawButton::CloseButton(theme_provider));
  g_signal_connect(close_button_->widget(), "clicked",
                   G_CALLBACK(OnClickCloseButtonThunk), this);
}

GtkWidget* OneClickSigninBubbleGtk::LayoutWidgets() {
  // Setup the BubbleGtk content.
  GtkWidget* content_widget = gtk_vbox_new(FALSE, ui::kContentAreaSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(content_widget),
                                 ui::kContentAreaBorder);
  if (header_label_) {
    GtkWidget* top_line = gtk_hbox_new(FALSE, ui::kControlSpacing);
    gtk_box_pack_start(GTK_BOX(top_line),
                       header_label_, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(top_line),
                     close_button_->widget(), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content_widget),
                       top_line, FALSE, FALSE, 0);
  }

  gtk_box_pack_start(GTK_BOX(content_widget),
                     message_label_, FALSE, FALSE, 0);

  if (learn_more_) {
    GtkWidget* box = gtk_hbox_new(FALSE, ui::kControlSpacing);
    gtk_box_pack_end(GTK_BOX(box),
                     learn_more_, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(content_widget),
                       box, TRUE, TRUE, 0);
  }

  GtkWidget* bottom_line = gtk_hbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(content_widget),
                     bottom_line, FALSE, FALSE, 0);

  gtk_box_pack_start(GTK_BOX(bottom_line),
                     advanced_link_, FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(bottom_line),
                   ok_button_, FALSE, FALSE, 0);
  gtk_box_pack_end(GTK_BOX(bottom_line),
                   undo_button_, FALSE, FALSE, 0);
  return content_widget;
}

void OneClickSigninBubbleGtk::ShowWidget(BrowserWindowGtk* browser_window_gtk,
                                         GtkWidget* content_widget) {
  GtkThemeService* const theme_provider = GtkThemeService::GetFrom(
      browser_window_gtk->browser()->profile());

  GtkWidget* parent_widget = is_modal_ ?
      browser_window_gtk->GetToolbar()->widget() :
      browser_window_gtk->GetToolbar()->GetAppMenuButton();
  gfx::Rect bounds = gtk_util::WidgetBounds(parent_widget);
  int flags = (is_modal_ ? BubbleGtk::NO_ACCELERATORS : BubbleGtk::GRAB_INPUT) |
              BubbleGtk::MATCH_SYSTEM_THEME |
              BubbleGtk::POPUP_WINDOW;
  bubble_ = BubbleGtk::Show(parent_widget, &bounds, content_widget,
                            is_modal_ ? BubbleGtk::CENTER_OVER_RECT :
                                        BubbleGtk::ANCHOR_TOP_RIGHT,
                            flags,
                            theme_provider, this);
  if (is_modal_) {
    gtk_window_set_transient_for(bubble_->GetNativeWindow(),
                                 browser_window_gtk->GetNativeWindow());
    gtk_window_set_modal(bubble_->GetNativeWindow(), true);
    gtk_window_set_focus(bubble_->GetNativeWindow(), ok_button_);
  } else {
    gtk_widget_grab_focus(ok_button_);
  }
}
