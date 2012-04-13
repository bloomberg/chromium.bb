// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_VIEW_MESSAGES_ENUMS_H_
#define CONTENT_COMMON_VIEW_MESSAGES_ENUMS_H_

#include "ipc/ipc_message_macros.h"

// Values that may be OR'd together to form the 'flags' parameter of a
// ViewHostMsg_UpdateRect_Params structure.
struct ViewHostMsg_UpdateRect_Flags {
  enum {
    IS_RESIZE_ACK = 1 << 0,
    IS_RESTORE_ACK = 1 << 1,
    IS_REPAINT_ACK = 1 << 2,
  };
  static bool is_resize_ack(int flags) {
    return (flags & IS_RESIZE_ACK) != 0;
  }
  static bool is_restore_ack(int flags) {
    return (flags & IS_RESTORE_ACK) != 0;
  }
  static bool is_repaint_ack(int flags) {
    return (flags & IS_REPAINT_ACK) != 0;
  }
};

struct ViewMsg_Navigate_Type {
 public:
  enum Value {
    // Reload the page.
    RELOAD,

    // Reload the page, ignoring any cache entries.
    RELOAD_IGNORING_CACHE,

    // The navigation is the result of session restore and should honor the
    // page's cache policy while restoring form state. This is set to true if
    // restoring a tab/session from the previous session and the previous
    // session did not crash. If this is not set and the page was restored then
    // the page's cache policy is ignored and we load from the cache.
    RESTORE,

    // Like RESTORE, except that the navigation contains POST data.
    RESTORE_WITH_POST,

    // Navigation type not categorized by the other types.
    NORMAL
  };
};

enum AccessibilityMode {
  // WebKit accessibility is off and no accessibility information is
  // sent from the renderer to the browser process.
  AccessibilityModeOff,

  // WebKit accessibility is on, but only limited information about
  // editable text nodes is sent to the browser process. Useful for
  // implementing limited UIA on tablets.
  AccessibilityModeEditableTextOnly,

  // WebKit accessibility is on, and the full accessibility tree is synced
  // to the browser process. Useful for screen readers and magnifiers.
  AccessibilityModeComplete,
};

#endif  // CONTENT_COMMON_VIEW_MESSAGES_ENUMS_H_
