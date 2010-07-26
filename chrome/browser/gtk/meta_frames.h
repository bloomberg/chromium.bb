// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_GTK_META_FRAMES_H_
#define CHROME_BROWSER_GTK_META_FRAMES_H_
#pragma once

#include <gdk/gdk.h>
#include <gtk/gtkwindow.h>

G_BEGIN_DECLS

// For the sake of gtk+ theme integration, we define a class called
// "MetaFrames," which is the name of a gobject class in the metacity window
// manager. To actually get at those values, we need to have an object whose
// gobject class name string matches the definitions in the gtkrc
// file. MetaFrames derives from GtkWindow.
//
// TODO(erg): http://crbug.com/35317 for getting rid of this hack class, as we
// should be able to use gtk_rc_get_style_by_path() but can't?

typedef struct _MetaFrames       MetaFrames;
typedef struct _MetaFramesClass  MetaFramesClass;

struct _MetaFrames {
  GtkWindow window;
};

struct _MetaFramesClass {
  GtkWindowClass parent_class;
};

// Creates a GtkWindow object with the class name "MetaFrames".
GtkWidget* meta_frames_new();

G_END_DECLS

#endif  // CHROME_BROWSER_GTK_META_FRAMES_H_
