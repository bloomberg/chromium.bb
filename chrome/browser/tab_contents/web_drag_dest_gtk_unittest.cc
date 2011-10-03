// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/tab_contents/web_drag_dest_gtk.h"

#include <gtk/gtk.h>

#include <cstring>

#include "base/memory/scoped_ptr.h"
#include "chrome/test/base/chrome_render_view_host_test_harness.h"
#include "content/browser/tab_contents/test_tab_contents.h"

class WebDragDestGtkTest : public ChromeRenderViewHostTestHarness {
 public:
  WebDragDestGtkTest() {}

 private:
  DISALLOW_COPY_AND_ASSIGN(WebDragDestGtkTest);
};

// Test that WebDragDestGtk doesn't crash when it gets drag events about a
// TabContents that doesn't have a corresponding TabContentsWrapper.  See
// http;//crosbug.com/20738.
TEST_F(WebDragDestGtkTest, NoTabContentsWrapper) {
  scoped_ptr<TestTabContents> tab_contents(CreateTestTabContents());
  GtkWidget* widget = gtk_button_new();
  g_object_ref_sink(widget);
  scoped_ptr<WebDragDestGtk> drag_dest(
      new WebDragDestGtk(tab_contents.get(), widget));

  // This is completely bogus and results in "Gtk-CRITICAL **:
  // gtk_drag_get_data: assertion `GDK_IS_DRAG_CONTEXT (context)' failed"
  // messages.  However, passing a correctly-typed GdkDragContext created with
  // g_object_new() results in a segfault, presumably because it's missing state
  // that GTK/GDK set up for real drags.
  GdkDragContext context;
  memset(&context, 0, sizeof(context));
  drag_dest->OnDragMotion(widget, &context, 0, 0, 0);  // x, y, time

  // This is bogus too.
  GtkSelectionData data;
  memset(&data, 0, sizeof(data));
  while (drag_dest->data_requests_ > 0) {
    drag_dest->OnDragDataReceived(widget,
                                  &context,
                                  0, 0,  // x, y
                                  &data,
                                  0, 0);  // info, time
  }

  // The next motion event after receiving all of the requested data is what
  // triggers the crash.
  drag_dest->OnDragMotion(widget, &context, 0, 0, 0);  // x, y, time

  drag_dest.reset();
  g_object_unref(widget);
}
