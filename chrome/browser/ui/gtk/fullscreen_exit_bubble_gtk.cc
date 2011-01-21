// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/fullscreen_exit_bubble_gtk.h"

#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_floating_container.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/rounded_window.h"
#include "grit/app_strings.h"
#include "grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

// Padding around the link text.
const int kPaddingPixels = 8;

// Time before the link slides away. This is a bit longer than the Windows
// timeout because we don't yet support reshowing when the mouse moves to the
// of the screen.
const int kInitialDelayMs = 3000;

// How long the slide up animation takes when hiding the bubble.
const int kSlideOutDurationMs = 700;

}

FullscreenExitBubbleGtk::FullscreenExitBubbleGtk(
    GtkFloatingContainer* container)
    : container_(container) {
  InitWidgets();
}

FullscreenExitBubbleGtk::~FullscreenExitBubbleGtk() {
}

void FullscreenExitBubbleGtk::InitWidgets() {
  // The exit bubble is a gtk_chrome_link_button inside a gtk event box and gtk
  // alignment (these provide the background color).  This is then made rounded
  // and put into a slide widget.

  // The Windows code actually looks up the accelerator key in the accelerator
  // table and then converts the key to a string (in a switch statement).
  std::string exit_text_utf8("<span color=\"white\" size=\"large\">");
  exit_text_utf8.append(l10n_util::GetStringFUTF8(
      IDS_EXIT_FULLSCREEN_MODE, l10n_util::GetStringUTF16(IDS_APP_F11_KEY)));
  exit_text_utf8.append("</span>");
  GtkWidget* link = gtk_chrome_link_button_new_with_markup(
      exit_text_utf8.c_str());
  gtk_chrome_link_button_set_use_gtk_theme(GTK_CHROME_LINK_BUTTON(link),
                                           FALSE);
  signals_.Connect(link, "clicked", G_CALLBACK(OnLinkClickedThunk), this);

  GtkWidget* container = gtk_util::CreateGtkBorderBin(
      link, &gtk_util::kGdkBlack,
      kPaddingPixels, kPaddingPixels, kPaddingPixels, kPaddingPixels);
  gtk_util::ActAsRoundedWindow(container, gtk_util::kGdkGreen, kPaddingPixels,
      gtk_util::ROUNDED_BOTTOM_LEFT | gtk_util::ROUNDED_BOTTOM_RIGHT,
      gtk_util::BORDER_NONE);

  slide_widget_.reset(new SlideAnimatorGtk(container,
      SlideAnimatorGtk::DOWN, kSlideOutDurationMs, false, false, NULL));
  gtk_widget_set_name(widget(), "exit-fullscreen-bubble");
  gtk_widget_show_all(container);
  gtk_widget_show(widget());
  slide_widget_->OpenWithoutAnimation();

  // TODO(tc): Implement the more complex logic in the windows version for
  // when to show/hide the exit bubble.
  initial_delay_.Start(base::TimeDelta::FromMilliseconds(kInitialDelayMs), this,
                       &FullscreenExitBubbleGtk::Hide);

  gtk_floating_container_add_floating(GTK_FLOATING_CONTAINER(container_),
                                      widget());
  signals_.Connect(container_, "set-floating-position",
                   G_CALLBACK(OnSetFloatingPositionThunk), this);
}

void FullscreenExitBubbleGtk::Hide() {
  slide_widget_->Close();
}

void FullscreenExitBubbleGtk::OnSetFloatingPosition(
    GtkWidget* floating_container,
    GtkAllocation* allocation) {
  GtkRequisition bubble_size;
  gtk_widget_size_request(widget(), &bubble_size);

  // Position the bubble at the top center of the screen.
  GValue value = { 0, };
  g_value_init(&value, G_TYPE_INT);
  g_value_set_int(&value, (allocation->width - bubble_size.width) / 2);
  gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                   widget(), "x", &value);

  g_value_set_int(&value, 0);
  gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                   widget(), "y", &value);
  g_value_unset(&value);
}

void FullscreenExitBubbleGtk::OnLinkClicked(GtkWidget* link) {
  GtkWindow* window = GTK_WINDOW(gtk_widget_get_toplevel(widget()));
  gtk_window_unfullscreen(window);
}
