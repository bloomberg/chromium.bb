// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ACCESSIBILITY_NOTIFICATION_H_
#define CONTENT_COMMON_ACCESSIBILITY_NOTIFICATION_H_

enum AccessibilityNotification {
  // The active descendant of a node has changed.
  AccessibilityNotificationActiveDescendantChanged,

  // An ARIA attribute changed, one that isn't covered by any
  // other notification.
  AccessibilityNotificationAriaAttributeChanged,

  // Autocorrect changed the text of a node.
  AccessibilityNotificationAutocorrectionOccurred,

  // An alert appeared.
  AccessibilityNotificationAlert,

  // A node has lost focus.
  AccessibilityNotificationBlur,

  // The node checked state has changed.
  AccessibilityNotificationCheckStateChanged,

  // The node tree structure has changed.
  AccessibilityNotificationChildrenChanged,

  // The node in focus has changed.
  AccessibilityNotificationFocusChanged,

  // Whether or not the node is invalid has changed.
  AccessibilityNotificationInvalidStatusChanged,

  // Page layout has completed.
  AccessibilityNotificationLayoutComplete,

  // Content within a part of the page marked as a live region changed.
  AccessibilityNotificationLiveRegionChanged,

  // The document node has loaded.
  AccessibilityNotificationLoadComplete,

  // A menu list selection changed.
  AccessibilityNotificationMenuListItemSelected,

  // A menu list value changed.
  AccessibilityNotificationMenuListValueChanged,

  // The object's accessible name changed.
  AccessibilityNotificationTextChanged,

  // An object was shown.
  AccessibilityNotificationObjectShow,

  // An object was hidden.
  AccessibilityNotificationObjectHide,

  // The number of rows in a grid or tree control changed.
  AccessibilityNotificationRowCountChanged,

  // A row in a grid or tree control was collapsed.
  AccessibilityNotificationRowCollapsed,

  // A row in a grid or tree control was expanded.
  AccessibilityNotificationRowExpanded,

  // The document was scrolled to an anchor node.
  AccessibilityNotificationScrolledToAnchor,

  // One or more selected children of this node have changed.
  AccessibilityNotificationSelectedChildrenChanged,

  // The text cursor or selection changed.
  AccessibilityNotificationSelectedTextChanged,

  // Text was inserted in a node with text content.
  AccessibilityNotificationTextInserted,

  // Text was removed in a node with text content.
  AccessibilityNotificationTextRemoved,

  // The node value has changed.
  AccessibilityNotificationValueChanged,
};

#endif  // CONTENT_COMMON_ACCESSIBILITY_NOTIFICATION_H_
