// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/one_click_signin_bubble_gtk.h"

#include <gtk/gtk.h>

#include "base/callback_helpers.h"
#include "base/i18n/rtl.h"
#include "base/logging.h"
#include "base/message_loop.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/tabs/tab_strip_gtk.h"
#include "chrome/browser/ui/sync/one_click_signin_helper.h"
#include "chrome/browser/ui/sync/one_click_signin_histogram.h"
#include "chrome/common/url_constants.h"
#include "grit/chromium_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/l10n/l10n_util.h"

static const int kModalDialogMessageWidth = 400;

OneClickSigninBubbleGtk::OneClickSigninBubbleGtk(
    BrowserWindowGtk* browser_window_gtk,
    BrowserWindow::OneClickSigninBubbleType type,
    const string16& email,
    const string16& error_message,
    const BrowserWindow::StartSyncCallback& start_sync_callback)
    : bubble_(NULL),
      email_(email),
      error_message_(error_message),
      start_sync_callback_(start_sync_callback),
      is_sync_dialog_(type!=BrowserWindow::ONE_CLICK_SIGNIN_BUBBLE_TYPE_BUBBLE),
      message_label_(NULL),
      advanced_link_(NULL),
      ok_button_(NULL),
      undo_button_(NULL),
      learn_more_(NULL),
      header_label_(NULL),
      clicked_learn_more_(false) {
  InitializeWidgets(browser_window_gtk);
  ShowWidget(browser_window_gtk, LayoutWidgets());
}

void OneClickSigninBubbleGtk::BubbleClosing(
    BubbleGtk* bubble, bool closed_by_escape) {
  if (is_sync_dialog_ && !start_sync_callback_.is_null()) {
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
  if (is_sync_dialog_) {
    OneClickSigninHelper::LogConfirmHistogramValue(
      clicked_learn_more_ ?
          one_click_signin::HISTOGRAM_CONFIRM_LEARN_MORE_ADVANCED :
          one_click_signin::HISTOGRAM_CONFIRM_ADVANCED);

    base::ResetAndReturn(&start_sync_callback_).Run(
      OneClickSigninSyncStarter::CONFIGURE_SYNC_FIRST);
  } else {
    Browser* browser = chrome::FindBrowserWithWindow(
      gtk_window_get_transient_for(bubble_->GetNativeWindow()));
    DCHECK(browser);
    chrome::NavigateParams params(browser, GURL(chrome::kChromeUISettingsURL),
        content::PAGE_TRANSITION_LINK);
    params.disposition = CURRENT_TAB;
    chrome::Navigate(&params);
  }
  bubble_->Close();
}

void OneClickSigninBubbleGtk::OnClickOK(GtkWidget* link) {
  if (is_sync_dialog_) {
    OneClickSigninHelper::LogConfirmHistogramValue(
      clicked_learn_more_ ?
          one_click_signin::HISTOGRAM_CONFIRM_LEARN_MORE_OK :
          one_click_signin::HISTOGRAM_CONFIRM_OK);

    base::ResetAndReturn(&start_sync_callback_).Run(
      OneClickSigninSyncStarter::SYNC_WITH_DEFAULT_SETTINGS);
  }
  bubble_->Close();
}

void OneClickSigninBubbleGtk::OnClickUndo(GtkWidget* link) {
  if (is_sync_dialog_) {
    OneClickSigninHelper::LogConfirmHistogramValue(
      clicked_learn_more_ ?
          one_click_signin::HISTOGRAM_CONFIRM_LEARN_MORE_UNDO :
          one_click_signin::HISTOGRAM_CONFIRM_UNDO);

    base::ResetAndReturn(&start_sync_callback_).Run(
        OneClickSigninSyncStarter::UNDO_SYNC);
  }
  bubble_->Close();
}

void OneClickSigninBubbleGtk::OnClickLearnMoreLink(GtkWidget* link) {
  // We only want to log the Learn More click once per modal dialog instance.
  if (is_sync_dialog_ && !clicked_learn_more_) {
    OneClickSigninHelper::LogConfirmHistogramValue(
        one_click_signin::HISTOGRAM_CONFIRM_LEARN_MORE);
    clicked_learn_more_ = true;
  }
  Browser* browser = chrome::FindBrowserWithWindow(
      gtk_window_get_transient_for(bubble_->GetNativeWindow()));
  DCHECK(browser);
  chrome::NavigateParams params(browser, GURL(chrome::kChromeSyncLearnMoreURL),
      content::PAGE_TRANSITION_LINK);
  params.disposition = NEW_WINDOW;
  chrome::Navigate(&params);

  if (!is_sync_dialog_) {
    bubble_->Close();
  }
}

void OneClickSigninBubbleGtk::OnClickCloseButton(GtkWidget* button) {
  OneClickSigninHelper::LogConfirmHistogramValue(
      clicked_learn_more_ ?
          one_click_signin::HISTOGRAM_CONFIRM_LEARN_MORE_CLOSE :
          one_click_signin::HISTOGRAM_CONFIRM_CLOSE);
  start_sync_callback_.Reset();
  bubble_->Close();
}

OneClickSigninBubbleGtk::~OneClickSigninBubbleGtk() {
}

void OneClickSigninBubbleGtk::InitializeWidgets(
    BrowserWindowGtk* browser_window_gtk) {
  // Main dialog/bubble message.
  std::string label_text;
  if (is_sync_dialog_) {
    label_text = email_.empty() ?
        l10n_util::GetStringUTF8(IDS_ONE_CLICK_SIGNIN_DIALOG_MESSAGE) :
        l10n_util::GetStringFUTF8(IDS_ONE_CLICK_SIGNIN_DIALOG_MESSAGE_NEW,
                                  email_);
  } else {
    label_text = !error_message_.empty() ? UTF16ToUTF8(error_message_):
        l10n_util::GetStringUTF8(IDS_ONE_CLICK_SIGNIN_BUBBLE_MESSAGE);
  }

  message_label_ = gtk_label_new(label_text.c_str());
  gtk_label_set_line_wrap(GTK_LABEL(message_label_), TRUE);
  gtk_misc_set_alignment(GTK_MISC(message_label_), 0.0, 0.5);
  if (is_sync_dialog_)
    gtk_widget_set_size_request(message_label_, kModalDialogMessageWidth, -1);

  GtkThemeService* const theme_provider = GtkThemeService::GetFrom(
      browser_window_gtk->browser()->profile());

  // Advanced link.
  advanced_link_ = theme_provider->BuildChromeLinkButton(
      l10n_util::GetStringUTF8(
          IDS_ONE_CLICK_SIGNIN_DIALOG_ADVANCED));
  g_signal_connect(advanced_link_, "clicked",
                   G_CALLBACK(OnClickAdvancedLinkThunk), this);

  // The 'Learn More...' link
  learn_more_ = theme_provider->BuildChromeLinkButton(
      l10n_util::GetStringUTF8(IDS_LEARN_MORE));
  g_signal_connect(learn_more_, "clicked",
                   G_CALLBACK(OnClickLearnMoreLinkThunk), this);

  // Make the OK and Undo buttons the same size horizontally.
  GtkSizeGroup* size_group = gtk_size_group_new(GTK_SIZE_GROUP_HORIZONTAL);
  // OK Button.
  ok_button_ = gtk_button_new_with_label(l10n_util::GetStringUTF8(
      is_sync_dialog_ ? IDS_ONE_CLICK_SIGNIN_DIALOG_OK_BUTTON :
                        IDS_OK).c_str());
  g_signal_connect(ok_button_, "clicked",
                   G_CALLBACK(OnClickOKThunk), this);
  gtk_size_group_add_widget(size_group, ok_button_);

  if (!is_sync_dialog_)
    return;

  // The undo button is only in the modal dialog
  undo_button_ = gtk_button_new_with_label(l10n_util::GetStringUTF8(
        IDS_ONE_CLICK_SIGNIN_DIALOG_UNDO_BUTTON).c_str());
  g_signal_connect(undo_button_, "clicked",
                   G_CALLBACK(OnClickUndoThunk), this);
  gtk_size_group_add_widget(size_group, undo_button_);

  g_object_unref(size_group);

  header_label_ = theme_provider->BuildLabel(email_.empty() ?
      l10n_util::GetStringUTF8(IDS_ONE_CLICK_SIGNIN_DIALOG_TITLE) :
      l10n_util::GetStringFUTF8(IDS_ONE_CLICK_SIGNIN_DIALOG_TITLE_NEW, email_),
      ui::kGdkBlack);

  PangoAttrList* attributes = pango_attr_list_new();
  pango_attr_list_insert(attributes,
                         pango_attr_weight_new(PANGO_WEIGHT_BOLD));
  gtk_label_set_attributes(GTK_LABEL(header_label_), attributes);
  pango_attr_list_unref(attributes);
  close_button_.reset(CustomDrawButton::CloseButtonBubble(theme_provider));
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
    if (is_sync_dialog_){
      gtk_box_pack_end(GTK_BOX(box),
                     learn_more_, FALSE, FALSE, 0);
    } else {
      gtk_box_pack_start(GTK_BOX(box),
                     learn_more_, FALSE, FALSE, 0);
    }
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

  if (is_sync_dialog_) {
    gtk_box_pack_end(GTK_BOX(bottom_line),
                     undo_button_, FALSE, FALSE, 0);
  }
  return content_widget;
}

void OneClickSigninBubbleGtk::ShowWidget(BrowserWindowGtk* browser_window_gtk,
                                         GtkWidget* content_widget) {
  if (is_sync_dialog_) {
    OneClickSigninHelper::LogConfirmHistogramValue(
        one_click_signin::HISTOGRAM_CONFIRM_SHOWN);
  }

  GtkThemeService* const theme_provider = GtkThemeService::GetFrom(
      browser_window_gtk->browser()->profile());

  GtkWidget* parent_widget = is_sync_dialog_ ?
      browser_window_gtk->GetToolbar()->widget() :
      browser_window_gtk->GetToolbar()->GetAppMenuButton();
  gfx::Rect bounds = gtk_util::WidgetBounds(parent_widget);
  int flags = (is_sync_dialog_ ? BubbleGtk::NO_ACCELERATORS :
                                 BubbleGtk::GRAB_INPUT) |
                                 BubbleGtk::MATCH_SYSTEM_THEME |
                                 BubbleGtk::POPUP_WINDOW;
  bubble_ = BubbleGtk::Show(parent_widget, &bounds, content_widget,
                            is_sync_dialog_ ? BubbleGtk::CENTER_OVER_RECT :
                                        BubbleGtk::ANCHOR_TOP_RIGHT,
                            flags,
                            theme_provider, this);

  gtk_window_set_transient_for(bubble_->GetNativeWindow(),
                                 browser_window_gtk->GetNativeWindow());
  if (is_sync_dialog_) {
    gtk_window_set_modal(bubble_->GetNativeWindow(), true);
    gtk_window_set_focus(bubble_->GetNativeWindow(), ok_button_);
  } else {
    gtk_widget_grab_focus(ok_button_);
  }
}
