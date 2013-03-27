// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/confirm_bubble_gtk.h"

#include <gtk/gtk.h>

#include "base/logging.h"
#include "base/utf_string_conversions.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/confirm_bubble.h"
#include "chrome/browser/ui/confirm_bubble_model.h"
#include "chrome/browser/ui/gtk/browser_window_gtk.h"
#include "chrome/browser/ui/gtk/custom_button.h"
#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_theme_service.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "grit/theme_resources.h"
#include "ui/base/gtk/gtk_hig_constants.h"
#include "ui/base/gtk/gtk_screen_util.h"
#include "ui/base/resource/resource_bundle.h"
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

// Maximum width for the message field. We will wrap the message text when its
// width is wider than this.
const int kMaxMessageWidth = 400;

}  // namespace

ConfirmBubbleGtk::ConfirmBubbleGtk(gfx::NativeView anchor,
                                   const gfx::Point& anchor_point,
                                   ConfirmBubbleModel* model)
    : bubble_(NULL),
      anchor_(anchor),
      anchor_point_(anchor_point),
      model_(model) {
  DCHECK(model);
}

ConfirmBubbleGtk::~ConfirmBubbleGtk() {
}

void ConfirmBubbleGtk::BubbleClosing(BubbleGtk* bubble,
                                     bool closed_by_escape) {
}

void ConfirmBubbleGtk::Show() {
  GtkWidget* toplevel = gtk_widget_get_toplevel(anchor_);
  BrowserWindowGtk* browser_window =
      BrowserWindowGtk::GetBrowserWindowForNativeWindow(GTK_WINDOW(toplevel));
  GtkThemeService* theme_service = GtkThemeService::GetFrom(
      browser_window->browser()->profile());

  GtkWidget* content = gtk_vbox_new(FALSE, kInterLineSpacing);
  gtk_container_set_border_width(GTK_CONTAINER(content), kContentBorder);
  g_signal_connect(content, "destroy", G_CALLBACK(OnDestroyThunk), this);

  // Add the icon, the title label and the close button to the first row.
  GtkWidget* row = gtk_hbox_new(FALSE, kImageViewSpacing);
  GtkWidget* icon_view =
      gtk_image_new_from_pixbuf(model_->GetIcon()->ToGdkPixbuf());
  gtk_box_pack_start(GTK_BOX(row), icon_view, FALSE, FALSE, 0);

  GtkWidget* title_label = theme_service->BuildLabel(
      UTF16ToUTF8(model_->GetTitle()), ui::kGdkBlack);
  gtk_box_pack_start(GTK_BOX(row), title_label, FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(row), gtk_label_new(NULL), TRUE, TRUE, 0);

  close_button_.reset(CustomDrawButton::CloseButtonBubble(theme_service));
  g_signal_connect(close_button_->widget(), "clicked",
                   G_CALLBACK(OnCloseButtonThunk), this);
  gtk_box_pack_end(GTK_BOX(row), close_button_->widget(), FALSE, FALSE, 0);
  gtk_box_pack_start(GTK_BOX(content), row, FALSE, FALSE, 0);

  // Add the message label to the second row.
  GtkWidget* message_label = theme_service->BuildLabel(
      UTF16ToUTF8(model_->GetMessageText()), ui::kGdkBlack);
  gtk_util::ForceFontSizePixels(message_label, kMessageTextSize);
  gtk_util::SetLabelWidth(message_label, kMaxMessageWidth);
  gtk_box_pack_start(GTK_BOX(content), message_label, FALSE, FALSE, 0);

  // Add the the link label to the third row if it exists.
  const string16 link_text = model_->GetLinkText();
  if (!link_text.empty()) {
    GtkWidget* row = gtk_hbox_new(FALSE, kImageViewSpacing);
    GtkWidget* link_button = gtk_chrome_link_button_new(
        UTF16ToUTF8(link_text).c_str());
    g_signal_connect(link_button, "clicked", G_CALLBACK(OnLinkButtonThunk),
                     this);
    gtk_util::ForceFontSizePixels(link_button, kMessageTextSize);
    gtk_box_pack_start(GTK_BOX(row), link_button, FALSE, FALSE, 0);
    gtk_box_pack_end(GTK_BOX(row), gtk_label_new(NULL), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(content), row, FALSE, FALSE, 0);
  }

  bool has_ok_button = !!(model_->GetButtons() & ConfirmBubbleModel::BUTTON_OK);
  bool has_cancel_button =
      !!(model_->GetButtons() & ConfirmBubbleModel::BUTTON_CANCEL);
  if (has_ok_button || has_cancel_button) {
    GtkWidget* row = gtk_hbox_new(FALSE, kImageViewSpacing);
    gtk_box_pack_start(GTK_BOX(row), gtk_label_new(NULL), TRUE, TRUE, 0);
    if (has_cancel_button) {
      GtkWidget* cancel_button = gtk_button_new_with_label(UTF16ToUTF8(
          model_->GetButtonLabel(ConfirmBubbleModel::BUTTON_CANCEL)).c_str());
      g_signal_connect(cancel_button, "clicked",
                       G_CALLBACK(OnCancelButtonThunk), this);
      gtk_box_pack_start(GTK_BOX(row), cancel_button, FALSE, FALSE, 0);
    }
    if (has_ok_button) {
      GtkWidget* ok_button = gtk_button_new_with_label(UTF16ToUTF8(
          model_->GetButtonLabel(ConfirmBubbleModel::BUTTON_OK)).c_str());
      g_signal_connect(ok_button, "clicked", G_CALLBACK(OnOkButtonThunk), this);
      gtk_box_pack_start(GTK_BOX(row), ok_button, FALSE, FALSE, 0);
    }
    gtk_box_pack_start(GTK_BOX(content), row, FALSE, FALSE, 0);
  }

  // Show a bubble consisting of the above widgets under the anchor point.
  gfx::Rect rect = ui::GetWidgetScreenBounds(anchor_);
  rect.set_x(anchor_point_.x() - rect.x());
  rect.set_y(anchor_point_.y() - rect.y());
  rect.set_width(0);
  rect.set_height(0);
  bubble_ = BubbleGtk::Show(anchor_,
                            &rect,
                            content,
                            BubbleGtk::FLOAT_BELOW_RECT,
                            BubbleGtk::MATCH_SYSTEM_THEME |
                                BubbleGtk::POPUP_WINDOW |
                                BubbleGtk::GRAB_INPUT,
                            theme_service,
                            this);  // error
}

void ConfirmBubbleGtk::OnDestroy(GtkWidget* sender) {
  // TODO(hbono): this code prevents the model from updating this view when we
  // click buttons. We should ask the model if we can delete this view.
  delete this;
}

void ConfirmBubbleGtk::OnCloseButton(GtkWidget* sender) {
  bubble_->Close();
}

void ConfirmBubbleGtk::OnOkButton(GtkWidget* sender) {
  model_->Accept();
  // TODO(hbono): this code prevents the model from updating this view when we
  // click this button. We should ask the model if we can close this view.
  bubble_->Close();
}

void ConfirmBubbleGtk::OnCancelButton(GtkWidget* sender) {
  model_->Cancel();
  // TODO(hbono): this code prevents the model from updating this view when we
  // click this button. We should ask the model if we can close this view.
  bubble_->Close();
}

void ConfirmBubbleGtk::OnLinkButton(GtkWidget* sender) {
  model_->LinkClicked();
  // TODO(hbono): this code prevents the model from updating this view when we
  // click this link. We should ask the model if we can close this view.
  bubble_->Close();
}

namespace chrome {

void ShowConfirmBubble(gfx::NativeView view,
                       const gfx::Point& origin,
                       ConfirmBubbleModel* model) {
  ConfirmBubbleGtk* bubble = new ConfirmBubbleGtk(view, origin, model);
  bubble->Show();
}

}  // namespace chrome
