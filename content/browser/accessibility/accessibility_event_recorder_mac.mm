// Copyright (c) 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/accessibility/accessibility_event_recorder.h"

#include <string>

#import <Cocoa/Cocoa.h>

#include "base/mac/foundation_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "content/browser/accessibility/browser_accessibility_manager.h"

namespace content {

// Implementation of AccessibilityEventRecorder that uses AXObserver to
// watch for NSAccessibility events.
class AccessibilityEventRecorderMac : public AccessibilityEventRecorder {
 public:
  explicit AccessibilityEventRecorderMac(BrowserAccessibilityManager* manager);
  ~AccessibilityEventRecorderMac() override;

  // Callback executed every time we receive an event notification.
  void EventReceived(AXUIElementRef element, CFStringRef notification);

 private:
  // Add one notification to the list of notifications monitored by our
  // observer.
  void AddNotification(NSString* notification);

  // Convenience function to get the value of an AX attribute from
  // an AXUIElementRef as a string.
  std::string GetAXAttributeValue(
      AXUIElementRef element, NSString* attribute_name);

  // The AXUIElement for the Chrome application.
  AXUIElementRef application_;

  // The AXObserver we use to monitor AX notifications.
  AXObserverRef observer_ref_;
};

// Callback function registered using AXObserverCreate.
static void EventReceivedThunk(
    AXObserverRef observer_ref,
    AXUIElementRef element,
    CFStringRef notification,
    void *refcon) {
  AccessibilityEventRecorderMac* this_ptr =
      static_cast<AccessibilityEventRecorderMac*>(refcon);
  this_ptr->EventReceived(element, notification);
}

// static
AccessibilityEventRecorder* AccessibilityEventRecorder::Create(
    BrowserAccessibilityManager* manager) {
  return new AccessibilityEventRecorderMac(manager);
}

AccessibilityEventRecorderMac::AccessibilityEventRecorderMac(
    BrowserAccessibilityManager* manager)
    : AccessibilityEventRecorder(manager),
      observer_ref_(0) {
  // Get Chrome's process id.
  int pid = [[NSProcessInfo processInfo] processIdentifier];
  if (kAXErrorSuccess != AXObserverCreate(
      pid, EventReceivedThunk, &observer_ref_)) {
    LOG(FATAL) << "Failed to create AXObserverRef";
  }

  // Get an AXUIElement for the Chrome application.
  application_ = AXUIElementCreateApplication(pid);
  if (!application_)
    LOG(FATAL) << "Failed to create AXUIElement for application.";

  // Add the notifications we care about to the observer.
  AddNotification(NSAccessibilityFocusedUIElementChangedNotification);
  AddNotification(NSAccessibilityRowCountChangedNotification);
  AddNotification(NSAccessibilitySelectedChildrenChangedNotification);
  AddNotification(NSAccessibilitySelectedRowsChangedNotification);
  AddNotification(NSAccessibilitySelectedTextChangedNotification);
  AddNotification(NSAccessibilityValueChangedNotification);
  AddNotification(@"AXLayoutComplete");
  AddNotification(@"AXLiveRegionChanged");
  AddNotification(@"AXLoadComplete");
  AddNotification(@"AXRowCollapsed");
  AddNotification(@"AXRowExpanded");

  // Add the observer to the current message loop.
  CFRunLoopAddSource(
      [[NSRunLoop currentRunLoop] getCFRunLoop],
      AXObserverGetRunLoopSource(observer_ref_),
      kCFRunLoopDefaultMode);
}

AccessibilityEventRecorderMac::~AccessibilityEventRecorderMac() {
  CFRelease(application_);
  CFRelease(observer_ref_);
}

void AccessibilityEventRecorderMac::AddNotification(NSString* notification) {
  AXObserverAddNotification(observer_ref_,
                            application_,
                            static_cast<CFStringRef>(notification),
                            this);
}

std::string AccessibilityEventRecorderMac::GetAXAttributeValue(
    AXUIElementRef element, NSString* attribute_name) {
  CFTypeRef value;
  AXError err = AXUIElementCopyAttributeValue(
      element, static_cast<CFStringRef>(attribute_name), &value);
  if (err != kAXErrorSuccess)
    return std::string();
  return base::SysNSStringToUTF8(
      base::mac::CFToNSCast(static_cast<CFStringRef>(value)));
}

void AccessibilityEventRecorderMac::EventReceived(
    AXUIElementRef element,
    CFStringRef notification) {
  std::string notification_str = base::SysNSStringToUTF8(
      base::mac::CFToNSCast(notification));
  std::string role = GetAXAttributeValue(element, NSAccessibilityRoleAttribute);
  if (role.empty())
    return;
  std::string log = base::StringPrintf("%s on %s",
      notification_str.c_str(), role.c_str());

  std::string title = GetAXAttributeValue(element,
                                          NSAccessibilityTitleAttribute);
  if (!title.empty())
    log += base::StringPrintf(" AXTitle=\"%s\"", title.c_str());

  std::string description = GetAXAttributeValue(element,
                                          NSAccessibilityDescriptionAttribute);
  if (!description.empty())
    log += base::StringPrintf(" AXDescription=\"%s\"", description.c_str());

  std::string value = GetAXAttributeValue(element,
                                          NSAccessibilityValueAttribute);
  if (!value.empty())
    log += base::StringPrintf(" AXValue=\"%s\"", value.c_str());

  event_logs_.push_back(log);
}

}  // namespace content
