// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/hover_controller_gtk.h"

#include "base/message_loop.h"
#include "chrome/browser/ui/gtk/gtk_chrome_button.h"

static const gchar* kHoverControllerGtkKey = "__HOVER_CONTROLLER_GTK__";

HoverControllerGtk::HoverControllerGtk(GtkWidget* button)
    : throb_animation_(this),
      hover_animation_(this),
      button_(button) {
  g_object_ref(button_);
  gtk_chrome_button_set_hover_state(GTK_CHROME_BUTTON(button_), 0);

  signals_.Connect(button_, "enter-notify-event",
                   G_CALLBACK(OnEnterThunk), this);
  signals_.Connect(button_, "leave-notify-event",
                   G_CALLBACK(OnLeaveThunk), this);
  signals_.Connect(button_, "destroy",
                   G_CALLBACK(OnDestroyThunk), this);
  signals_.Connect(button_, "hierarchy-changed",
                   G_CALLBACK(OnHierarchyChangedThunk), this);

#ifndef NDEBUG
  if (g_object_get_data(G_OBJECT(button_), kHoverControllerGtkKey))
    NOTREACHED();
#endif  // !NDEBUG

  g_object_set_data(G_OBJECT(button), kHoverControllerGtkKey, this);
}

HoverControllerGtk::~HoverControllerGtk() {
}

void HoverControllerGtk::StartThrobbing(int cycles) {
  throb_animation_.StartThrobbing(cycles);
}

// static
GtkWidget* HoverControllerGtk::CreateChromeButton() {
  GtkWidget* widget = gtk_chrome_button_new();
  new HoverControllerGtk(widget);
  return widget;
}

// static
HoverControllerGtk* HoverControllerGtk::GetHoverControllerGtk(
    GtkWidget* button) {
  return reinterpret_cast<HoverControllerGtk*>(
      g_object_get_data(G_OBJECT(button), kHoverControllerGtkKey));
}

void HoverControllerGtk::Destroy() {
  gtk_chrome_button_set_hover_state(GTK_CHROME_BUTTON(button_), -1.0);

  g_object_set_data(G_OBJECT(button_), kHoverControllerGtkKey, NULL);
  g_object_unref(button_);
  button_ = NULL;

  delete this;
}

void HoverControllerGtk::AnimationProgressed(const ui::Animation* animation) {
  if (!button_)
    return;

  // Ignore the hover animation if we are throbbing.
  if (animation == &hover_animation_ && throb_animation_.is_animating())
    return;

  gtk_chrome_button_set_hover_state(GTK_CHROME_BUTTON(button_),
                                    animation->GetCurrentValue());
}

void HoverControllerGtk::AnimationEnded(const ui::Animation* animation) {
  if (!button_)
    return;
  if (animation != &throb_animation_)
    return;

  if (throb_animation_.cycles_remaining() <= 1)
    gtk_chrome_button_set_hover_state(GTK_CHROME_BUTTON(button_), 0);
}

void HoverControllerGtk::AnimationCanceled(const ui::Animation* animation) {
  AnimationEnded(animation);
}

gboolean HoverControllerGtk::OnEnter(GtkWidget* widget,
                                     GdkEventCrossing* event) {
  hover_animation_.Show();

  return FALSE;
}

gboolean HoverControllerGtk::OnLeave(GtkWidget* widget,
                                     GdkEventCrossing* event) {
  // When the user is holding a mouse button, we don't want to animate.
  if (event->state & (GDK_BUTTON1_MASK | GDK_BUTTON2_MASK | GDK_BUTTON3_MASK)) {
    hover_animation_.Reset();
    gtk_chrome_button_set_hover_state(GTK_CHROME_BUTTON(button_), 0);
  } else {
    hover_animation_.Hide();
  }

  return FALSE;
}

void HoverControllerGtk::OnHierarchyChanged(GtkWidget* widget,
                                            GtkWidget* previous_toplevel) {
  // GTK+ does not emit leave-notify-event signals when a widget
  // becomes unanchored, so manually unset the hover states.
  if (!GTK_WIDGET_TOPLEVEL(gtk_widget_get_toplevel(widget))) {
    gtk_widget_set_state(button_, GTK_STATE_NORMAL);
    hover_animation_.Reset();
    gtk_chrome_button_set_hover_state(GTK_CHROME_BUTTON(button_), 0.0);
  }
}

void HoverControllerGtk::OnDestroy(GtkWidget* widget) {
  Destroy();
}
