// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/owned_widget_gtk.h"

#include <gtk/gtk.h>

#include "base/debug_util.h"
#include "base/logging.h"
#include "base/message_loop.h"

namespace {

class CheckWidgetRefCountTask : public Task {
 public:
  CheckWidgetRefCountTask(GtkWidget* widget, const std::string& stack)
      : widget_(widget), stack_(stack) {}

  virtual ~CheckWidgetRefCountTask() {}

  virtual void Run() {
    // NOTE: Assumes some implementation details about glib internals.
    DCHECK_EQ(G_OBJECT(widget_)->ref_count, 1U) <<
        " with Destroy() stack: \n" << stack_;
    g_object_unref(widget_);
  }

 private:
  GtkWidget* widget_;
  std::string stack_;
};

}  // namespace

OwnedWidgetGtk::~OwnedWidgetGtk() {
  DCHECK(!widget_) << "You must explicitly call OwnerWidgetGtk::Destroy().";
}

void OwnedWidgetGtk::Own(GtkWidget* widget) {
  if (!widget)
    return;

  DCHECK(!widget_);
  // We want to make sure that Own() was called properly, right after the
  // widget was created. There should be a floating reference.
  DCHECK(g_object_is_floating(widget));

  // Sink the floating reference, we should now own this reference.
  g_object_ref_sink(widget);
  widget_ = widget;
}

void OwnedWidgetGtk::Destroy() {
  if (!widget_)
    return;

  GtkWidget* widget = widget_;
  widget_ = NULL;
  gtk_widget_destroy(widget);

#ifndef NDEBUG
  DCHECK(!g_object_is_floating(widget));

  std::string stack(StackTrace().AsString());
#else
  std::string stack;
#endif

  // Make sure all other ref holders let go of their references (but delay
  // the check because some ref holders won't let go immediately).
  MessageLoop::current()->PostTask(
      FROM_HERE, new CheckWidgetRefCountTask(widget, stack));
}
