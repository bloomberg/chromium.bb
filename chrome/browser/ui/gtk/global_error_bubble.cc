// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/global_error_bubble.h"

#include <gtk/gtk.h>

#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_error/global_error.h"
#include "chrome/browser/ui/global_error/global_error_service.h"
#include "chrome/browser/ui/global_error/global_error_service_factory.h"
#include "chrome/browser/ui/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image/image.h"

namespace {

// Text size of the message label. 12.1px = 9pt @ 96dpi.
const double kMessageTextSize = 12.1;

// Minimum width of the message label.
const int kMinMessageLabelWidth = 250;

}  // namespace

GlobalErrorBubble::GlobalErrorBubble(Browser* browser,
                                     const base::WeakPtr<GlobalError>& error,
                                     GtkWidget* anchor)
    : browser_(browser),
      bubble_(NULL),
      error_(error),
      message_width_(kMinMessageLabelWidth) {
  DCHECK(browser_);
  DCHECK(error_);
  GtkWidget* content = gtk_vbox_new(FALSE, ui::kControlSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(content),
                                 ui::kContentAreaBorder);
  g_signal_connect(content, "destroy", G_CALLBACK(OnDestroyThunk), this);

  GtkThemeService* theme_service =
      GtkThemeService::GetFrom(browser_->profile());

  int resource_id = error_->GetBubbleViewIconResourceID();
  ui::ResourceBundle& rb = ui::ResourceBundle::GetSharedInstance();
  GdkPixbuf* pixbuf = rb.GetNativeImageNamed(resource_id).ToGdkPixbuf();
  GtkWidget* image_view = gtk_image_new_from_pixbuf(pixbuf);

  GtkWidget* title_label = theme_service->BuildLabel(
      UTF16ToUTF8(error_->GetBubbleViewTitle()),
      ui::kGdkBlack);
  std::vector<string16> messages = error_->GetBubbleViewMessages();
  for (size_t i = 0; i < messages.size(); ++i) {
    message_labels_.push_back(theme_service->BuildLabel(
        UTF16ToUTF8(messages[i]),
        ui::kGdkBlack));
    gtk_util::ForceFontSizePixels(message_labels_[i], kMessageTextSize);
  }
  // Message label will be sized later in "realize" callback because we need
  // to know the width of the title and the width of the buttons group.
  GtkWidget* accept_button = gtk_button_new_with_label(
      UTF16ToUTF8(error_->GetBubbleViewAcceptButtonLabel()).c_str());
  string16 cancel_string = error_->GetBubbleViewCancelButtonLabel();
  GtkWidget* cancel_button = NULL;
  if (!cancel_string.empty()) {
    cancel_button =
        gtk_button_new_with_label(UTF16ToUTF8(cancel_string).c_str());
  }

  // Top, icon and title.
  GtkWidget* top = gtk_hbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(top), image_view, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(top), title_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(content), top, FALSE, FALSE, 0);

  // Middle, messages.
  for (size_t i = 0; i < message_labels_.size(); ++i)
    gtk_box_pack_start(GTK_BOX(content), message_labels_[i], FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(content), gtk_hbox_new(FALSE, 0), FALSE, FALSE, 0);

  // Bottom, accept and cancel button.
  // We want the buttons on the right, so just use an expanding label to fill
  // all of the extra space on the left.
  GtkWidget* bottom = gtk_hbox_new(FALSE, ui::kControlSpacing);
  gtk_box_pack_start(GTK_BOX(bottom), gtk_label_new(NULL), TRUE, TRUE, 0);
  if (cancel_button)
    gtk_box_pack_start(GTK_BOX(bottom), cancel_button, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(bottom), accept_button, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(content), bottom, FALSE, FALSE, 0);

  gtk_widget_grab_focus(accept_button);

  g_signal_connect(accept_button, "clicked",
                   G_CALLBACK(OnAcceptButtonThunk), this);
  if (cancel_button) {
    g_signal_connect(cancel_button, "clicked",
                     G_CALLBACK(OnCancelButtonThunk), this);
  }

  g_signal_connect(top, "realize", G_CALLBACK(OnRealizeThunk), this);
  g_signal_connect(bottom, "realize", G_CALLBACK(OnRealizeThunk), this);

  bubble_ = BubbleGtk::Show(anchor,
                            NULL,
                            content,
                            BubbleGtk::ANCHOR_TOP_RIGHT,
                            BubbleGtk::MATCH_SYSTEM_THEME |
                                BubbleGtk::POPUP_WINDOW |
                                BubbleGtk::GRAB_INPUT,
                            theme_service,
                            this);  // error_
}

GlobalErrorBubble::~GlobalErrorBubble() {
}

void GlobalErrorBubble::BubbleClosing(BubbleGtk* bubble,
                                      bool closed_by_escape) {
  if (error_)
    error_->BubbleViewDidClose(browser_);
}

void GlobalErrorBubble::OnDestroy(GtkWidget* sender) {
  delete this;
}

void GlobalErrorBubble::OnAcceptButton(GtkWidget* sender) {
  if (error_)
    error_->BubbleViewAcceptButtonPressed(browser_);
  bubble_->Close();
}

void GlobalErrorBubble::OnCancelButton(GtkWidget* sender) {
  if (error_)
    error_->BubbleViewCancelButtonPressed(browser_);
  bubble_->Close();
}

void GlobalErrorBubble::OnRealize(GtkWidget* sender) {
  int width = gtk_util::GetWidgetSize(sender).width();
  message_width_ = std::max(message_width_, width);
  for (size_t i = 0; i < message_labels_.size(); ++i)
    gtk_util::SetLabelWidth(message_labels_[i], message_width_);
}

void GlobalErrorBubble::CloseBubbleView() {
  bubble_->Close();
}

GlobalErrorBubbleViewBase* GlobalErrorBubbleViewBase::ShowBubbleView(
    Browser* browser,
    const base::WeakPtr<GlobalError>& error) {
  BrowserWindowGtk* browser_window =
      BrowserWindowGtk::GetBrowserWindowForNativeWindow(
          browser->window()->GetNativeWindow());
  GtkWidget* anchor = browser_window->GetToolbar()->GetAppMenuButton();

  // The bubble will be automatically deleted when it's closed.
  return new GlobalErrorBubble(browser, error, anchor);
}
