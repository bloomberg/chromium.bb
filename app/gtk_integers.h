// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef APP_GTK_INTEGERS_H_
#define APP_GTK_INTEGERS_H_

// GLib/Gobject/Gtk all use their own integer typedefs. They are copied here
// for forward declaration reasons so we don't pull in all of gtk.h when we
// just need a gpointer.
typedef char gchar;
typedef short gshort;
typedef long glong;
typedef int gint;
typedef gint gboolean;
typedef unsigned char guchar;
typedef unsigned short gushort;
typedef unsigned long gulong;
typedef unsigned int guint;

typedef unsigned short guint16;
typedef unsigned int guint32;

typedef void* gpointer;
typedef const void *gconstpointer;

#endif  // APP_GTK_INTEGERS_H_
