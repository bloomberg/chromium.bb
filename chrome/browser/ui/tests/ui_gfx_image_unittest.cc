// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/scoped_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/image.h"
#include "ui/gfx/image_unittest.h"

#if defined(OS_LINUX)
#include <gtk/gtk.h>
#include "ui/gfx/gtk_util.h"
#endif

#if defined(TOOLKIT_VIEWS)
#include "views/controls/image_view.h"
#include "views/view.h"
#endif

namespace {

using namespace ui::gfx::test;

#if defined(TOOLKIT_VIEWS)
TEST(UiGfxImageTest, ViewsImageView) {
  ui::gfx::Image image(CreatePlatformImage());

  scoped_ptr<views::View> container(new views::View());
  container->SetBounds(0, 0, 200, 200);
  container->SetVisible(true);

  scoped_ptr<views::ImageView> image_view(new views::ImageView());
  image_view->SetImage(image);
  container->AddChildView(image_view.get());
}
#endif

#if defined(OS_LINUX)
TEST(UiGfxImageTest, GtkImageView) {
  GtkWidget* window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
  gtk_window_resize(GTK_WINDOW(window), 200, 200);
  gtk_window_move(GTK_WINDOW(window), 300, 300);

  GtkWidget* fixed = gtk_fixed_new();
  gtk_container_add(GTK_CONTAINER(window), fixed);

  ui::gfx::Image image(CreateBitmap());
  GtkWidget* image_view = gtk_image_new_from_pixbuf(image);
  gtk_fixed_put(GTK_FIXED(fixed), image_view, 10, 10);
  gtk_widget_set_size_request(image_view, 25, 25);

  gtk_widget_show_all(window);

  gtk_widget_destroy(window);
}
#endif

}  // namespace
