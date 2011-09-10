// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/global_error_bubble.h"

#include <gtk/gtk.h>

#include "base/i18n/rtl.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/global_error.h"
#include "chrome/browser/ui/gtk/browser_toolbar_gtk.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/resource/resource_bundle.h"
#include "ui/gfx/gtk_util.h"
#include "ui/gfx/image/image.h"

namespace {

// Padding between content and edge of bubble.
const int kContentBorder = 7;

// Horizontal spacing between the image view and the label.
const int kImageViewSpacing = 5;

// Vertical spacing between labels.
const int kInterLineSpacing = 5;

// Text size of the message label. 12.1px = 9pt @ 96dpi.
const double kMessageTextSize = 12.1;

// Width of the message label.
const int kMessageLabelWidth = 250;

}  // namespace

GlobalErrorBubble::GlobalErrorBubble(Profile* profile,
                                     GlobalError* error,
                                     GtkWidget* anchor)
    : bubble_(NULL),
      error_(error) {
  GtkWidget* content = gtk_vbox_new(FALSE, kInterLineSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(content), kContentBorder);
  g_signal_connect(content, "destroy", G_CALLBACK(OnDestroyThunk), this);

  GtkThemeService* theme_service = GtkThemeService::GetFrom(profile);

  int resource_id = error_->GetBubbleViewIconResourceID();
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  GdkPixbuf* pixbuf = rb.GetNativeImageNamed(resource_id).ToGdkPixbuf();
  GtkWidget* image_view = gtk_image_new_from_pixbuf(pixbuf);

  GtkWidget* title_label = theme_service->BuildLabel(
      UTF16ToUTF8(error_->GetBubbleViewTitle()),
      ui::kGdkBlack);
  GtkWidget* message_label = theme_service->BuildLabel(
      UTF16ToUTF8(error_->GetBubbleViewMessage()),
      ui::kGdkBlack);
  gtk_util::ForceFontSizePixels(message_label, kMessageTextSize);
  gtk_util::SetLabelWidth(message_label, kMessageLabelWidth);
  GtkWidget* accept_button = gtk_button_new_with_label(
      UTF16ToUTF8(error_->GetBubbleViewAcceptButtonLabel()).c_str());
  string16 cancel_string = error_->GetBubbleViewCancelButtonLabel();
  GtkWidget* cancel_button = NULL;
  if (!cancel_string.empty()) {
    cancel_button =
        gtk_button_new_with_label(UTF16ToUTF8(cancel_string).c_str());
  }

  // Top, icon and title.
  GtkWidget* top = gtk_hbox_new(FALSE, kImageViewSpacing);
  gtk_box_pack_start(GTK_BOX(top), image_view, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(top), title_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(content), top, FALSE, FALSE, 0);

  // Middle, message.
  gtk_box_pack_start(GTK_BOX(content), message_label, FALSE, FALSE, 0);
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

  BubbleGtk::ArrowLocationGtk arrow_location =
      base::i18n::IsRTL() ?
      BubbleGtk::ARROW_LOCATION_TOP_LEFT :
      BubbleGtk::ARROW_LOCATION_TOP_RIGHT;
  bubble_ = BubbleGtk::Show(anchor,
                            NULL,
                            content,
                            arrow_location,
                            true,  // match_system_theme
                            true,  // grab_input
                            theme_service,
                            this);  // error_
}

GlobalErrorBubble::~GlobalErrorBubble() {
}

void GlobalErrorBubble::BubbleClosing(BubbleGtk* bubble,
                                      bool closed_by_escape) {
  error_->BubbleViewDidClose();
}

void GlobalErrorBubble::OnDestroy(GtkWidget* sender) {
  delete this;
}

void GlobalErrorBubble::OnAcceptButton(GtkWidget* sender) {
  error_->BubbleViewAcceptButtonPressed();
  bubble_->Close();
}

void GlobalErrorBubble::OnCancelButton(GtkWidget* sender) {
  error_->BubbleViewCancelButtonPressed();
  bubble_->Close();
}

void GlobalError::ShowBubbleView(Browser* browser, GlobalError* error) {
  BrowserWindowGtk* browser_window =
      BrowserWindowGtk::GetBrowserWindowForNativeWindow(
          browser->window()->GetNativeHandle());
  GtkWidget* anchor = browser_window->GetToolbar()->GetAppMenuButton();

  // The bubble will be automatically deleted when it's closed.
  new GlobalErrorBubble(browser->profile(), error, anchor);
}
