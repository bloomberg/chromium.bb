// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/meta_frames.h"

G_BEGIN_DECLS

G_DEFINE_TYPE(MetaFrames, meta_frames, GTK_TYPE_WINDOW)

static void meta_frames_class_init(MetaFramesClass* frames_class) {
  // Noop since we don't declare anything.
}

static void meta_frames_init(MetaFrames* button) {
}

GtkWidget* meta_frames_new(void) {
  GtkWindow* window =
      GTK_WINDOW(g_object_new(meta_frames_get_type(), NULL));
  window->type = GTK_WINDOW_TOPLEVEL;
  return GTK_WIDGET(window);
}

G_END_DECLS
