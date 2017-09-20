// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <Cocoa/Cocoa.h>

#include "base/mac/foundation_util.h"
#include "base/mac/mac_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/cocoa/view_id_util.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "testing/gtest_mac.h"

class AccessibilityTest : public InProcessBrowserTest {
 public:
  AccessibilityTest() {}

 protected:
  NSView* ContentView() {
    return [browser()->window()->GetNativeWindow() contentView];
  }

  NSView* ViewWithID(ViewID id) {
    return view_id_util::GetView(browser()->window()->GetNativeWindow(), id);
  }

  bool AXHasAttribute(id obj, NSString* attribute) {
    return [[obj accessibilityAttributeNames] containsObject:attribute];
  }

  // For NSView subclasses with cells, the cell contains some of the
  // accessibility properties, not the view. NSButton is one such subclass;
  // there are probably also others. This function returns the correct object to
  // ask for |attribute| on |object|, or |object| if there is no such correct
  // object.
  id RealAXObjectFor(id object, NSString* attribute) {
    if ([object isKindOfClass:[NSButton class]]) {
      NSButtonCell* cell = [static_cast<NSButton*>(object) cell];
      if (AXHasAttribute(cell, attribute))
        return cell;
    }
    return object;
  }

  id AXAttribute(id obj, NSString* attribute) {
    return
        [RealAXObjectFor(obj, attribute) accessibilityAttributeValue:attribute];
  }

  bool AXIsIgnored(id obj) { return [obj accessibilityIsIgnored]; }

  NSString* AXRole(id obj) {
    return base::mac::ObjCCast<NSString>(
        AXAttribute(obj, NSAccessibilityRoleAttribute));
  }

  NSArray* AXChildren(id obj) {
    return base::mac::ObjCCast<NSArray>(
        AXAttribute(obj, NSAccessibilityChildrenAttribute));
  }

  NSString* AXTitle(id obj) {
    return base::mac::ObjCCast<NSString>(
        AXAttribute(obj, NSAccessibilityTitleAttribute));
  }

  NSString* AXDescription(id obj) {
    return base::mac::ObjCCast<NSString>(
        AXAttribute(obj, NSAccessibilityDescriptionAttribute));
  }

  // Returns an array containing every non-ignored accessibility node that is a
  // descendant of |root|.
  NSArray* CollectAXTree(id root) {
    NSMutableArray* result = [[[NSMutableArray alloc] init] autorelease];
    CollectAXTreeHelper(root, result);
    return result;
  }

 private:
  void CollectAXTreeHelper(id root, NSMutableArray* result) {
    if (!AXIsIgnored(root))
      [result addObject:root];
    for (id child : AXChildren(root))
      CollectAXTreeHelper(child, result);
  }
};

IN_PROC_BROWSER_TEST_F(AccessibilityTest, EveryElementHasRole) {
  // These a11y APIs are not present in 10.9, so these tests don't run there.
  if (!base::mac::IsAtLeastOS10_10())
    return;
  NSArray* elements = CollectAXTree(ContentView());
  EXPECT_GT([elements count], 0U);
  for (id elem : elements) {
    EXPECT_NSNE(AXRole(elem), NSAccessibilityUnknownRole);
  }
}

// Every element must have at least one of a title or a description.
IN_PROC_BROWSER_TEST_F(AccessibilityTest, EveryElementHasText) {
  if (!base::mac::IsAtLeastOS10_10())
    return;
  NSArray* elements = CollectAXTree(ContentView());
  EXPECT_GT([elements count], 0U);
  for (id elem : elements) {
    NSString* text =
        [AXTitle(elem) length] > 0 ? AXTitle(elem) : AXDescription(elem);
    EXPECT_NSNE(text, @"");
  }
}

IN_PROC_BROWSER_TEST_F(AccessibilityTest, ControlRoles) {
  if (!base::mac::IsAtLeastOS10_10())
    return;
  // TODO(ellyjones): figure out a more principled way to ensure each control
  // has the expected role. For now, this is just a regression test for
  // https://crbug.com/754223
  EXPECT_NSEQ(AXRole(ViewWithID(VIEW_ID_APP_MENU)),
              NSAccessibilityPopUpButtonRole);
}
