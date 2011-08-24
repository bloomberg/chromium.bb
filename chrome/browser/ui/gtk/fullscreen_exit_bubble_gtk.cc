// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/fullscreen_exit_bubble_gtk.h"

#include "chrome/browser/ui/gtk/gtk_chrome_link_button.h"
#include "chrome/browser/ui/gtk/gtk_util.h"
#include "chrome/browser/ui/gtk/rounded_window.h"
#include "grit/generated_resources.h"
#include "grit/ui_strings.h"
#include "ui/base/gtk/gtk_floating_container.h"
#include "ui/base/l10n/l10n_util.h"

FullscreenExitBubbleGtk::FullscreenExitBubbleGtk(
    GtkFloatingContainer* container,
    CommandUpdater::CommandUpdaterDelegate* delegate)
    : FullscreenExitBubble(delegate),
      container_(container) {
  InitWidgets();
  StartWatchingMouse();
}

FullscreenExitBubbleGtk::~FullscreenExitBubbleGtk() {
}

void FullscreenExitBubbleGtk::InitWidgets() {
  // The exit bubble is a gtk_chrome_link_button inside a gtk event box and gtk
  // alignment (these provide the background color).  This is then made rounded
  // and put into a slide widget.

  // The Windows code actually looks up the accelerator key in the accelerator
  // table and then converts the key to a string (in a switch statement). This
  // doesn't seem to be implemented for Gtk, so we just use F11 directly.
  std::string exit_text_utf8("<span color=\"white\" size=\"large\">");
  exit_text_utf8.append(l10n_util::GetStringFUTF8(
      IDS_EXIT_FULLSCREEN_MODE, l10n_util::GetStringUTF16(IDS_APP_F11_KEY)));
  exit_text_utf8.append("</span>");
  GtkWidget* link = gtk_chrome_link_button_new_with_markup(
      exit_text_utf8.c_str());
  gtk_chrome_link_button_set_use_gtk_theme(GTK_CHROME_LINK_BUTTON(link),
                                           FALSE);
  signals_.Connect(link, "clicked", G_CALLBACK(OnLinkClickedThunk), this);

  link_container_.Own(gtk_util::CreateGtkBorderBin(
      link, &gtk_util::kGdkBlack,
      kPaddingPx, kPaddingPx, kPaddingPx, kPaddingPx));
  gtk_util::ActAsRoundedWindow(link_container_.get(), gtk_util::kGdkGreen,
      kPaddingPx,
      gtk_util::ROUNDED_BOTTOM_LEFT | gtk_util::ROUNDED_BOTTOM_RIGHT,
      gtk_util::BORDER_NONE);

  slide_widget_.reset(new SlideAnimatorGtk(link_container_.get(),
      SlideAnimatorGtk::DOWN, kSlideOutDurationMs, false, false, NULL));
  gtk_widget_set_name(widget(), "exit-fullscreen-bubble");
  gtk_widget_show_all(link_container_.get());
  gtk_widget_show(widget());
  slide_widget_->OpenWithoutAnimation();

  gtk_floating_container_add_floating(GTK_FLOATING_CONTAINER(container_),
                                      widget());
  signals_.Connect(container_, "set-floating-position",
                   G_CALLBACK(OnSetFloatingPositionThunk), this);
}

gfx::Rect FullscreenExitBubbleGtk::GetPopupRect(
    bool ignore_animation_state) const {
  GtkRequisition bubble_size;
  if (ignore_animation_state) {
    gtk_widget_size_request(link_container_.get(), &bubble_size);
  } else {
    gtk_widget_size_request(widget(), &bubble_size);
  }
  return gfx::Rect(bubble_size.width, bubble_size.height);
}

gfx::Point FullscreenExitBubbleGtk::GetCursorScreenPoint() {
  GdkDisplay* display = gtk_widget_get_display(widget());

  // Get cursor position.
  // TODO: this hits the X server, so we may want to consider decreasing
  // kPositionCheckHz if we detect that we're running remotely.
  int x, y;
  gdk_display_get_pointer(display, NULL, &x, &y, NULL);

  return gfx::Point(x, y);
}

bool FullscreenExitBubbleGtk::WindowContainsPoint(gfx::Point pos) {
  GtkWindow* window = GTK_WINDOW(
      gtk_widget_get_ancestor(widget(), GTK_TYPE_WINDOW));
  int width, height, x, y;
  gtk_window_get_size(window, &width, &height);
  gtk_window_get_position(window, &x, &y);
  return gfx::Rect(x, y, width, height).Contains(pos);
}

bool FullscreenExitBubbleGtk::IsWindowActive() {
  if (!widget()->parent)
    return false;
  GtkWindow* window = GTK_WINDOW(
      gtk_widget_get_ancestor(widget(), GTK_TYPE_WINDOW));
  return gtk_window_is_active(window);
}

void FullscreenExitBubbleGtk::Hide() {
  slide_widget_->Close();
}

void FullscreenExitBubbleGtk::Show() {
  slide_widget_->Open();
}

bool FullscreenExitBubbleGtk::IsAnimating() {
  return slide_widget_->IsAnimating();
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
  ToggleFullscreen();
}
