// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/fullscreen_exit_bubble_gtk.h"

#include "app/gfx/gtk_util.h"
#include "app/l10n_util.h"
#include "chrome/browser/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/gtk/gtk_floating_container.h"
#include "chrome/browser/gtk/rounded_window.h"
#include "chrome/common/gtk_util.h"
#include "grit/app_strings.h"
#include "grit/generated_resources.h"

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
  // If the user exits the browser while in fullscreen mode, we may already
  // have been removed from the widget hierarchy.
  GtkWidget* parent = gtk_widget_get_parent(widget());
  if (parent) {
    DCHECK_EQ(GTK_WIDGET(container_), parent);
    g_signal_handlers_disconnect_by_func(parent,
        reinterpret_cast<gpointer>(OnSetFloatingPosition), this);
    gtk_container_remove(GTK_CONTAINER(parent), widget());
  }
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
  g_signal_connect(link, "clicked", G_CALLBACK(OnLinkClicked), this);

  GtkWidget* container = gtk_util::CreateGtkBorderBin(link, &gfx::kGdkBlack,
      kPaddingPixels, kPaddingPixels, kPaddingPixels, kPaddingPixels);
  gtk_util::ActAsRoundedWindow(container, gfx::kGdkGreen, kPaddingPixels,
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
  g_signal_connect(container_, "set-floating-position",
      G_CALLBACK(OnSetFloatingPosition), this);
}

void FullscreenExitBubbleGtk::Hide() {
  slide_widget_->Close();
}

// static
void FullscreenExitBubbleGtk::OnSetFloatingPosition(
    GtkFloatingContainer* floating_container, GtkAllocation* allocation,
    FullscreenExitBubbleGtk* bubble) {
  GtkRequisition bubble_size;
  gtk_widget_size_request(bubble->widget(), &bubble_size);

  // Position the bubble at the top center of the screen.
  GValue value = { 0, };
  g_value_init(&value, G_TYPE_INT);
  g_value_set_int(&value, (allocation->width - bubble_size.width) / 2);
  gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                   bubble->widget(), "x", &value);

  g_value_set_int(&value, 0);
  gtk_container_child_set_property(GTK_CONTAINER(floating_container),
                                   bubble->widget(), "y", &value);
  g_value_unset(&value);
}

// static
void FullscreenExitBubbleGtk::OnLinkClicked(GtkWidget* link,
                                            FullscreenExitBubbleGtk* bubble) {
  GtkWindow* window = GTK_WINDOW(gtk_widget_get_toplevel(
      bubble->widget()));
  gtk_window_unfullscreen(window);
}
