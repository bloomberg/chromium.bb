// Copyright (c) 2010 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/gtk/accessibility_event_router_gtk.h"
#include "chrome/test/testing_profile.h"
#include "testing/gtest/include/gtest/gtest.h"

class AccessibilityEventRouterGtkTest : public testing::Test {
 protected:
  AccessibilityEventRouterGtkTest() { }
};

TEST_F(AccessibilityEventRouterGtkTest, AddRootWidgetTwice) {
  AccessibilityEventRouterGtk* event_router =
      AccessibilityEventRouterGtk::GetInstance();
  TestingProfile profile;

  GtkWidget* widget = gtk_window_new(GTK_WINDOW_TOPLEVEL);

  bool found = false;
  event_router->FindWidget(widget, NULL, &found);
  EXPECT_FALSE(found);

  event_router->AddRootWidget(widget, &profile);
  event_router->FindWidget(widget, NULL, &found);
  EXPECT_TRUE(found);

  event_router->AddRootWidget(widget, &profile);
  event_router->FindWidget(widget, NULL, &found);
  EXPECT_TRUE(found);

  event_router->RemoveRootWidget(widget);
  event_router->FindWidget(widget, NULL, &found);
  EXPECT_TRUE(found);

  event_router->RemoveRootWidget(widget);
  event_router->FindWidget(widget, NULL, &found);
  EXPECT_FALSE(found);

  gtk_widget_destroy(widget);
};
