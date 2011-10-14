// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_GTK_G_OBJECT_WEAK_REF_H_
#define CHROME_BROWSER_UI_GTK_G_OBJECT_WEAK_REF_H_
#pragma once

#include <gtk/gtk.h>

#include "base/memory/ref_counted.h"

// GObjectWeakRef is like base::WeakRef, but for GLib objects. It can be useful
// when passing GObject/GtkWidget arguments to closures (e.g. base::Bind()) that
// may not necessarily outlast the closure. It is not thread-safe. Use like so:
//
// void DelayedAction(const GObjectWeakRef& object) {
//   if (!object.get()) return;
//   GtkWidget* widget = GTK_WIDGET(object.get());
//   // code using |widget|
// }
//
// GtkWidget* widget = ...;
// MessageLoop::current()->PostTask(
//     FROM_HERE, base::Bind(&DelayedAction, GObjectWeakRef(widget)));

class GObjectWeakRef {
 public:
  // GObjectWeakRef is just a thin wrapper around GObjectWeakRefDelegate that
  // can be copied and assigned, even though the delegate is reference counted.
  explicit GObjectWeakRef(GObject* object);
  explicit GObjectWeakRef(GtkWidget* widget);
  GObjectWeakRef(const GObjectWeakRef& ref);  // NOLINT: explicitly non-explicit
  ~GObjectWeakRef();

  // Assignment operators for GObject, GtkWidget, and GObjectWeakRef.
  GObjectWeakRef& operator=(GObject* object);
  GObjectWeakRef& operator=(GtkWidget* widget);
  GObjectWeakRef& operator=(const GObjectWeakRef& ref);

  GObject* get() const;

 private:
  class GObjectWeakRefDelegate;

  void reset(GObject* object);

  scoped_refptr<GObjectWeakRefDelegate> ref_;
};

#endif  // CHROME_BROWSER_UI_GTK_G_OBJECT_WEAK_REF_H_
