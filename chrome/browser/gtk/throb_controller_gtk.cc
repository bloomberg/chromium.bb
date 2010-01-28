// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/throb_controller_gtk.h"

#include "base/message_loop.h"
#include "chrome/browser/gtk/gtk_chrome_button.h"

static const gchar* kThrobControllerGtkKey = "__THROB_CONTROLLER_GTK__";

ThrobControllerGtk::ThrobControllerGtk(GtkWidget* button)
    : animation_(this),
      button_(button) {
  g_object_ref(button_);
  g_signal_connect(button_, "destroy", G_CALLBACK(OnButtonDestroy), this);

#ifndef NDEBUG
  if (g_object_get_data(G_OBJECT(button_), kThrobControllerGtkKey))
    NOTREACHED();
#endif  // !NDEBUG

  g_object_set_data(G_OBJECT(button), kThrobControllerGtkKey, this);
}

ThrobControllerGtk::~ThrobControllerGtk() {
}

void ThrobControllerGtk::StartThrobbing(int cycles) {
  animation_.StartThrobbing(cycles);
}

// static
ThrobControllerGtk* ThrobControllerGtk::GetThrobControllerGtk(
    GtkWidget* button) {
  return reinterpret_cast<ThrobControllerGtk*>(
      g_object_get_data(G_OBJECT(button), kThrobControllerGtkKey));
}

// static
void ThrobControllerGtk::ThrobFor(GtkWidget* button) {
  if (!GTK_IS_CHROME_BUTTON(button)) {
    NOTREACHED();
    return;
  }

  (new ThrobControllerGtk(button))->
      StartThrobbing(std::numeric_limits<int>::max());
}

void ThrobControllerGtk::Destroy() {
  gtk_chrome_button_set_hover_state(GTK_CHROME_BUTTON(button_), -1.0);
  g_signal_handlers_disconnect_by_func(
      button_,
      reinterpret_cast<gpointer>(OnButtonDestroy),
      this);
  g_object_set_data(G_OBJECT(button_), kThrobControllerGtkKey, NULL);
  g_object_unref(button_);
  button_ = NULL;

  // Since this can be called from within AnimationEnded(), which is called
  // while ThrobAnimation is still doing work, we need to let the stack unwind
  // before |animation_| gets deleted.
  MessageLoop::current()->DeleteSoon(FROM_HERE, this);
}

void ThrobControllerGtk::AnimationProgressed(const Animation* animation) {
  if (!button_)
    return;

  gtk_chrome_button_set_hover_state(GTK_CHROME_BUTTON(button_),
                                    animation->GetCurrentValue());
}

void ThrobControllerGtk::AnimationEnded(const Animation* animation) {
  if (!button_)
    return;

  if (animation_.cycles_remaining() <= 1)
    Destroy();
}

void ThrobControllerGtk::AnimationCanceled(const Animation* animation) {
  if (!button_)
    return;

  if (animation_.cycles_remaining() <= 1)
    Destroy();
}

// static
void ThrobControllerGtk::OnButtonDestroy(GtkWidget* widget,
                                         ThrobControllerGtk* button) {
  button->Destroy();
}
