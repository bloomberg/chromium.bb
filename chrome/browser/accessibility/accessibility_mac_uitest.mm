// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Cocoa/Cocoa.h>

#include "base/memory/scoped_nsobject.h"
#include "base/test/test_timeouts.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/ui/ui_test.h"

// The following tests exercise the Chrome Mac accessibility implementation
// similarly to the way in which VoiceOver would.
// We achieve this by utilizing the same carbon API (HIServices) as do
// other assistive technologies.
// Note that the tests must be UITests since these API's only work if not
// called within the same process begin examined.
class AccessibilityMacUITest : public UITest {
 public:
  AccessibilityMacUITest() {
    // TODO(dtseng): fake the VoiceOver defaults value?
    launch_arguments_.AppendSwitch(switches::kForceRendererAccessibility);
  }

  virtual void SetUp() {
    UITest::SetUp();
    SetupObservedNotifications();
    Initialize();
  }

  // Called to insert an event for validation.
  // This is a order sensitive expectation.
  void AddExpectedEvent(NSString* notificationName) {
    [AccessibilityMacUITest::expectedEvents addObject:notificationName];
  }

  // Assert that there are no remaining expected events.
  // CFRunLoop necessary to receive AX callbacks.
  // Assumes that there is at least one expected event.
  // The runloop stops only if we receive all expected notifications.
  void WaitAndAssertAllEventsObserved() {
    ASSERT_GE([expectedEvents count], 1U);
    CFRunLoopRunInMode(
        kCFRunLoopDefaultMode,
        TestTimeouts::action_max_timeout_ms() / 1000, false);
    ASSERT_EQ(0U, [AccessibilityMacUITest::expectedEvents count]);
  }

  // The Callback handler added to each AXUIElement.
  static void EventReceiver(
      AXObserverRef observerRef,
      AXUIElementRef element,
      CFStringRef notificationName,
      void *refcon) {
    if ([[AccessibilityMacUITest::expectedEvents objectAtIndex:0]
            isEqualToString:(NSString*)notificationName]) {
      [AccessibilityMacUITest::expectedEvents removeObjectAtIndex:0];
    }

    if ([AccessibilityMacUITest::expectedEvents count] == 0) {
      CFRunLoopStop(CFRunLoopGetCurrent());
    }

    // TODO(dtseng): currently refreshing on all notifications; scope later.
    AccessibilityMacUITest::SetAllObserversOnDescendants(
        element, observerRef);
  }

 private:
  // Perform AX setup.
  void Initialize() {
    AccessibilityMacUITest::expectedEvents.reset([[NSMutableArray alloc] init]);

    // Construct the Chrome AXUIElementRef.
    ASSERT_NE(-1, browser_process_id());
    AXUIElementRef browserUiElement =
        AXUIElementCreateApplication(browser_process_id());
    ASSERT_TRUE(browserUiElement);

    // Setup our callbacks.
    AXObserverRef observerRef;
    ASSERT_EQ(kAXErrorSuccess,
              AXObserverCreate(browser_process_id(),
                               AccessibilityMacUITest::EventReceiver,
                               &observerRef));
    SetAllObserversOnDescendants(browserUiElement, observerRef);

    // Add the observer to the current message loop.
    CFRunLoopAddSource(
        [[NSRunLoop currentRunLoop] getCFRunLoop],
        AXObserverGetRunLoopSource(observerRef),
        kCFRunLoopDefaultMode);
  }

  // Taken largely from AXNotificationConstants.h
  // (substituted NSAccessibility* to avoid casting).
  static void SetupObservedNotifications() {
    AccessibilityMacUITest::observedNotifications.reset(
        [[NSArray alloc] initWithObjects:

            // focus notifications
            NSAccessibilityMainWindowChangedNotification,
            NSAccessibilityFocusedWindowChangedNotification,
            NSAccessibilityFocusedUIElementChangedNotification,

            // application notifications
            NSAccessibilityApplicationActivatedNotification,
            NSAccessibilityApplicationDeactivatedNotification,
            NSAccessibilityApplicationHiddenNotification,
            NSAccessibilityApplicationShownNotification,

            // window notifications
            NSAccessibilityWindowCreatedNotification,
            NSAccessibilityWindowMovedNotification,
            NSAccessibilityWindowResizedNotification,
            NSAccessibilityWindowMiniaturizedNotification,
            NSAccessibilityWindowDeminiaturizedNotification,

            // new drawer, sheet, and help tag notifications
            NSAccessibilityDrawerCreatedNotification,
            NSAccessibilitySheetCreatedNotification,
            NSAccessibilityHelpTagCreatedNotification,

            // element notifications
            NSAccessibilityValueChangedNotification,
            NSAccessibilityUIElementDestroyedNotification,

            // menu notifications
            (NSString*)kAXMenuOpenedNotification,
            (NSString*)kAXMenuClosedNotification,
            (NSString*)kAXMenuItemSelectedNotification,

            // table/outline notifications
            NSAccessibilityRowCountChangedNotification,

            // other notifications
            NSAccessibilitySelectedChildrenChangedNotification,
            NSAccessibilityResizedNotification,
            NSAccessibilityMovedNotification,
            NSAccessibilityCreatedNotification,
            NSAccessibilitySelectedRowsChangedNotification,
            NSAccessibilitySelectedColumnsChangedNotification,
            NSAccessibilitySelectedTextChangedNotification,
            NSAccessibilityTitleChangedNotification,

            // Webkit specific notifications.
            @"AXLoadComplete",
            nil]);
      }

  // Observe AX notifications on element and all descendants.
  static void SetAllObserversOnDescendants(
      AXUIElementRef element,
      AXObserverRef observerRef) {
    SetAllObservers(element, observerRef);
    CFTypeRef childrenRef;
    if ((AXUIElementCopyAttributeValue(
            element, kAXChildrenAttribute, &childrenRef)) == kAXErrorSuccess) {
      NSArray* children = (NSArray*)childrenRef;
      for (uint32 i = 0; i < [children count]; ++i) {
        SetAllObserversOnDescendants(
            (AXUIElementRef)[children objectAtIndex:i], observerRef);
      }
    }
  }

  // Add observers for all notifications we know about.
  static void SetAllObservers(
      AXUIElementRef element,
      AXObserverRef observerRef) {
    for (NSString* notification in
         AccessibilityMacUITest::observedNotifications.get()) {
      AXObserverAddNotification(
          observerRef, element, (CFStringRef)notification, nil);
    }
  }

  // Used to keep track of events received during the lifetime of the tests.
  static scoped_nsobject<NSMutableArray> expectedEvents;
  // NSString collection of all AX notifications.
  static scoped_nsobject<NSArray> observedNotifications;
};

scoped_nsobject<NSMutableArray> AccessibilityMacUITest::expectedEvents;
scoped_nsobject<NSArray> AccessibilityMacUITest::observedNotifications;

TEST_F(AccessibilityMacUITest, TestInitialPageNotifications) {
  // Browse to a new page.
  GURL tree_url(
      "data:text/html,<html><head><title>Accessibility Mac Test</title></head>"
      "<body><input type='button' value='push' /><input type='checkbox' />"
      "</body></html>");
  NavigateToURLAsync(tree_url);

  // Test for navigation.
  AddExpectedEvent(@"AXLoadComplete");

  // Check all the expected Mac notifications.
  WaitAndAssertAllEventsObserved();
}
