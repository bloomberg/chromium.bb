// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/gtk/gtk_chrome_shrinkable_hbox.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace {

const int kSpacing = 3;
const int kBorderWidth = 5;

}  // namespace

class GtkChromeShrinkableHBoxTest : public testing::Test {
 protected:
  GtkChromeShrinkableHBoxTest()
      : window_(gtk_window_new(GTK_WINDOW_TOPLEVEL)),
        box_(gtk_chrome_shrinkable_hbox_new(FALSE, FALSE, kSpacing)) {
    gtk_widget_set_direction(box_, GTK_TEXT_DIR_LTR);
    gtk_window_set_default_size(GTK_WINDOW(window_), 200, 200);
    gtk_container_add(GTK_CONTAINER(window_), box_);
    gtk_container_set_border_width(GTK_CONTAINER(box_), kBorderWidth);
  }

  ~GtkChromeShrinkableHBoxTest() {
    gtk_widget_destroy(window_);
  }

  // Add some children widgets with arbitrary width and padding.
  void AddChildren(bool pack_start) {
    static struct {
      int width;
      int padding;
    } kChildrenData[] = {
      { 60, 2 },
      { 70, 3 },
      { 80, 5 },
      { 50, 7 },
      { 40, 11 },
      { 60, 0 },
      { 0, 0 }
    };

    for (size_t i = 0; kChildrenData[i].width; ++i) {
      GtkWidget* child = gtk_fixed_new();
      gtk_widget_set_size_request(child, kChildrenData[i].width, -1);
      if (pack_start) {
        gtk_chrome_shrinkable_hbox_pack_start(
            GTK_CHROME_SHRINKABLE_HBOX(box_), child, kChildrenData[i].padding);
      } else {
        gtk_chrome_shrinkable_hbox_pack_end(
            GTK_CHROME_SHRINKABLE_HBOX(box_), child, kChildrenData[i].padding);
      }
    }
  }

  // Check if all children's size allocation are inside the |box_|'s boundary.
  void Validate(bool pack_start) {
    std::vector<ChildData> children_data;
    gtk_container_foreach(GTK_CONTAINER(box_), CollectChildData,
                          &children_data);

    size_t children_count = children_data.size();
    size_t visible_children_count = 0;
    for (size_t i = 0; i < children_count; ++i) {
      if (children_data[i].visible)
        ++visible_children_count;
    }

    if (visible_children_count == 0)
      return;

    int border_width = gtk_container_get_border_width(GTK_CONTAINER(box_));
    int x = box_->allocation.x + border_width;
    int width = box_->allocation.width - border_width * 2;
    int spacing = gtk_box_get_spacing(GTK_BOX(box_));
    bool homogeneous = gtk_box_get_homogeneous(GTK_BOX(box_));

    if (homogeneous) {
      // If the |box_| is in homogeneous mode, then check if the visible
      // children are not overlapped with each other.
      int homogeneous_child_width =
          (width - (visible_children_count - 1) * spacing) /
          visible_children_count;

      for (size_t i = 0; i < children_count; ++i) {
        SCOPED_TRACE(testing::Message() << "Validate homogeneous child " << i
                     << " visible: " << children_data[i].visible
                     << " padding: " << children_data[i].padding
                     << " x: " << children_data[i].x
                     << " width: " << children_data[i].width);

        if (children_data[i].visible)
          ASSERT_LE(children_data[i].width, homogeneous_child_width);
      }
    } else {
      // If the |box_| is not in homogeneous mode, then just check if all
      // visible children are inside the |box_|'s boundary. And for those
      // hidden children which are out of the boundary, they should only
      // be hidden one by one from the end of the |box_|.
      bool last_visible = pack_start;
      bool visibility_changed = false;
      for (size_t i = 0; i < children_count; ++i) {
        SCOPED_TRACE(testing::Message() << "Validate child " << i
                     << " visible: " << children_data[i].visible
                     << " padding: " << children_data[i].padding
                     << " x: " << children_data[i].x
                     << " width: " << children_data[i].width);

        if (last_visible != children_data[i].visible) {
          ASSERT_FALSE(visibility_changed);
          visibility_changed = true;
          last_visible = children_data[i].visible;
        }
        if (children_data[i].visible) {
          ASSERT_GE(children_data[i].x,
                    x + children_data[i].padding);
          ASSERT_LE(children_data[i].x + children_data[i].width,
                    x + width - children_data[i].padding);
        }
      }
    }
  }

  void Test(bool pack_start) {
    gtk_widget_show_all(window_);
    GtkAllocation allocation = { 0, 0, 0, 200 };
    gtk_chrome_shrinkable_hbox_set_hide_child_directly(
        GTK_CHROME_SHRINKABLE_HBOX(box_), FALSE);
    for (int width = 500; width > kBorderWidth * 2; --width) {
      SCOPED_TRACE(testing::Message() << "Shrink hide_child_directly = FALSE,"
                   << " width = " << width);

      allocation.width = width;
      // Reducing the width may cause some children to be hidden, which will
      // cause queue resize, so it's necessary to do another size allocation to
      // emulate the queue resize.
      gtk_widget_size_allocate(box_, &allocation);
      gtk_widget_size_allocate(box_, &allocation);
      ASSERT_NO_FATAL_FAILURE(Validate(pack_start)) << "width = " << width;
    }

    for (int width = kBorderWidth * 2; width <= 500; ++width) {
      SCOPED_TRACE(testing::Message() << "Expand hide_child_directly = FALSE,"
                   << " width = " << width);

      allocation.width = width;
      // Expanding the width may cause some invisible children to be shown,
      // which will cause queue resize, so it's necessary to do another size
      // allocation to emulate the queue resize.
      gtk_widget_size_allocate(box_, &allocation);
      gtk_widget_size_allocate(box_, &allocation);
      ASSERT_NO_FATAL_FAILURE(Validate(pack_start));
    }

    gtk_chrome_shrinkable_hbox_set_hide_child_directly(
        GTK_CHROME_SHRINKABLE_HBOX(box_), TRUE);
    for (int width = 500; width > kBorderWidth * 2; --width) {
      SCOPED_TRACE(testing::Message() << "Shrink hide_child_directly = TRUE,"
                   << " width = " << width);

      allocation.width = width;
      gtk_widget_size_allocate(box_, &allocation);
      gtk_widget_size_allocate(box_, &allocation);
      ASSERT_NO_FATAL_FAILURE(Validate(pack_start));
    }

    for (int width = kBorderWidth * 2; width <= 500; ++width) {
      SCOPED_TRACE(testing::Message() << "Expand hide_child_directly = TRUE,"
                   << " width = " << width);

      allocation.width = width;
      gtk_widget_size_allocate(box_, &allocation);
      gtk_widget_size_allocate(box_, &allocation);
      ASSERT_NO_FATAL_FAILURE(Validate(pack_start));
    }
  }

 protected:
  GtkWidget* window_;
  GtkWidget* box_;

 private:
  struct ChildData {
    bool visible;
    int padding;
    int x;
    int width;
  };

  static void CollectChildData(GtkWidget* child, gpointer userdata) {
    guint padding;
    gtk_box_query_child_packing(GTK_BOX(gtk_widget_get_parent(child)), child,
                                NULL, NULL, &padding, NULL);

    ChildData data;
    data.visible = GTK_WIDGET_VISIBLE(child);
    data.padding = padding;
    data.x = child->allocation.x;
    data.width = child->allocation.width;

    reinterpret_cast<std::vector<ChildData>*>(userdata)->push_back(data);
  }
};

TEST_F(GtkChromeShrinkableHBoxTest, PackStart) {
  AddChildren(true);

  {
    SCOPED_TRACE("Test LTR");
    gtk_widget_set_direction(box_, GTK_TEXT_DIR_LTR);
    EXPECT_NO_FATAL_FAILURE(Test(true));
  }
  {
    SCOPED_TRACE("Test RTL");
    gtk_widget_set_direction(box_, GTK_TEXT_DIR_RTL);
    EXPECT_NO_FATAL_FAILURE(Test(true));
  }
}

TEST_F(GtkChromeShrinkableHBoxTest, PackEnd) {
  AddChildren(false);

  {
    SCOPED_TRACE("Test LTR");
    gtk_widget_set_direction(box_, GTK_TEXT_DIR_LTR);
    EXPECT_NO_FATAL_FAILURE(Test(false));
  }
  {
    SCOPED_TRACE("Test RTL");
    gtk_widget_set_direction(box_, GTK_TEXT_DIR_RTL);
    EXPECT_NO_FATAL_FAILURE(Test(false));
  }
}

TEST_F(GtkChromeShrinkableHBoxTest, Homogeneous) {
  AddChildren(true);
  gtk_box_set_homogeneous(GTK_BOX(box_), true);

  {
    SCOPED_TRACE("Test LTR");
    gtk_widget_set_direction(box_, GTK_TEXT_DIR_LTR);
    EXPECT_NO_FATAL_FAILURE(Test(true));
  }
  {
    SCOPED_TRACE("Test RTL");
    gtk_widget_set_direction(box_, GTK_TEXT_DIR_RTL);
    EXPECT_NO_FATAL_FAILURE(Test(true));
  }
}
