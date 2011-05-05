// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import "chrome/browser/ui/panels/panel_browser_window_cocoa.h"

#import <Cocoa/Cocoa.h>

#include "base/debug/debugger.h"
#include "base/memory/scoped_ptr.h"
#import "chrome/browser/ui/cocoa/cocoa_test_helper.h"
#import "chrome/browser/ui/cocoa/browser_test_helper.h"
#include "chrome/browser/ui/panels/panel.h"
#include "chrome/browser/ui/panels/panel_manager.h"
#include "testing/gtest/include/gtest/gtest.h"

// Main test class.
class PanelBrowserWindowCocoaTest : public CocoaTest {
 protected:
  BrowserTestHelper browser_helper_;
};

TEST_F(PanelBrowserWindowCocoaTest, CreateClose) {
  PanelManager* manager = PanelManager::GetInstance();
  EXPECT_EQ(0, manager->active_count());  // No panels initially.

  scoped_ptr<Panel> panel(manager->CreatePanel(browser_helper_.browser()));
  EXPECT_TRUE(panel.get());
  EXPECT_TRUE(panel->browser_window());  // Native panel is created right away.
  PanelBrowserWindowCocoa* native_window =
      static_cast<PanelBrowserWindowCocoa*>(panel->browser_window());

  EXPECT_EQ(panel, native_window->panel_);  // Back pointer initialized.
  EXPECT_EQ(1, manager->active_count());
  // BrowserTestHelper provides a browser w/o window_ set.
  // Use Browser::set_window() if needed.
  EXPECT_EQ(NULL, browser_helper_.browser()->window());

  // Window should not load before Show()
  EXPECT_FALSE([native_window->controller_ isWindowLoaded]);
  panel->Show();
  EXPECT_TRUE([native_window->controller_ isWindowLoaded]);
  EXPECT_TRUE([native_window->controller_ window]);

  gfx::Rect bounds = panel->GetBounds();
  EXPECT_TRUE(bounds.width() > 0);
  EXPECT_TRUE(bounds.height() > 0);

  // NSWindows created by NSWindowControllers don't have this bit even if
  // their NIB has it. The controller's lifetime is the window's lifetime.
  EXPECT_EQ(NO, [[native_window->controller_ window] isReleasedWhenClosed]);

  panel->Close();
  EXPECT_EQ(0, manager->active_count());
  // Close() destroys the controller, which destroys the NSWindow. CocoaTest
  // base class verifies that there is no remaining open windows after the test.
  EXPECT_FALSE(native_window->controller_);
}

TEST_F(PanelBrowserWindowCocoaTest, AssignedBounds) {
  PanelManager* manager = PanelManager::GetInstance();
  scoped_ptr<Panel> panel1(manager->CreatePanel(browser_helper_.browser()));
  scoped_ptr<Panel> panel2(manager->CreatePanel(browser_helper_.browser()));
  scoped_ptr<Panel> panel3(manager->CreatePanel(browser_helper_.browser()));
  panel1->Show();
  panel2->Show();
  panel3->Show();

  gfx::Rect bounds1 = panel1->GetBounds();
  gfx::Rect bounds2 = panel2->GetBounds();
  gfx::Rect bounds3 = panel3->GetBounds();

  // This checks panelManager calculating and assigning bounds right.
  // Panels should stack on the bottom right to left.
  EXPECT_LT(bounds3.x() + bounds3.width(), bounds2.x());
  EXPECT_LT(bounds2.x() + bounds2.width(), bounds1.x());
  EXPECT_EQ(bounds1.y(), bounds2.y());
  EXPECT_EQ(bounds2.y(), bounds3.y());

  panel2->Close();
  bounds3 = panel3->GetBounds();
  // After panel2 is closed, panel3 should take its place.
  EXPECT_EQ(bounds2, bounds3);
}

// Same test as AssignedBounds, but checks actual bounds on native OS windows.
TEST_F(PanelBrowserWindowCocoaTest, NativeBounds) {
  PanelManager* manager = PanelManager::GetInstance();
  scoped_ptr<Panel> panel1(manager->CreatePanel(browser_helper_.browser()));
  scoped_ptr<Panel> panel2(manager->CreatePanel(browser_helper_.browser()));
  scoped_ptr<Panel> panel3(manager->CreatePanel(browser_helper_.browser()));
  panel1->Show();
  panel2->Show();
  panel3->Show();

  PanelBrowserWindowCocoa* native_window1 =
      static_cast<PanelBrowserWindowCocoa*>(panel1->browser_window());
  PanelBrowserWindowCocoa* native_window2 =
      static_cast<PanelBrowserWindowCocoa*>(panel2->browser_window());
  PanelBrowserWindowCocoa* native_window3 =
      static_cast<PanelBrowserWindowCocoa*>(panel3->browser_window());

  NSRect bounds1 = [[native_window1->controller_ window] frame];
  NSRect bounds2 = [[native_window2->controller_ window] frame];
  NSRect bounds3 = [[native_window3->controller_ window] frame];

  EXPECT_LT(bounds3.origin.x + bounds3.size.width, bounds2.origin.x);
  EXPECT_LT(bounds2.origin.x + bounds2.size.width, bounds1.origin.x);
  EXPECT_EQ(bounds1.origin.y, bounds2.origin.y);
  EXPECT_EQ(bounds2.origin.y, bounds3.origin.y);

  panel2->Close();
  bounds3 = [[native_window3->controller_ window] frame];
  // After panel2 is closed, panel3 should take its place.
  EXPECT_EQ(bounds2.origin.x, bounds3.origin.x);
  EXPECT_EQ(bounds2.origin.y, bounds3.origin.y);
  EXPECT_EQ(bounds2.size.width, bounds3.size.width);
  EXPECT_EQ(bounds2.size.height, bounds3.size.height);
}

