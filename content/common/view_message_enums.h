// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_VIEW_MESSAGES_ENUMS_H_
#define CONTENT_COMMON_VIEW_MESSAGES_ENUMS_H_

struct ViewHostMsg_AccEvent {
  enum Value {
    // The active descendant of a node has changed.
    ACTIVE_DESCENDANT_CHANGED,

    // An alert appeared.
    ALERT,

    // The node checked state has changed.
    CHECK_STATE_CHANGED,

    // The node tree structure has changed.
    CHILDREN_CHANGED,

    // The node in focus has changed.
    FOCUS_CHANGED,

    // Page layout has completed.
    LAYOUT_COMPLETE,

    // Content within a part of the page marked as a live region changed.
    LIVE_REGION_CHANGED,

    // The document node has loaded.
    LOAD_COMPLETE,

    // A menu list value changed.
    MENU_LIST_VALUE_CHANGED,

    // An object was shown.
    OBJECT_SHOW,

    // An object was hidden.
    OBJECT_HIDE,

    // The number of rows in a grid or tree control changed.
    ROW_COUNT_CHANGED,

    // A row in a grid or tree control was collapsed.
    ROW_COLLAPSED,

    // A row in a grid or tree control was expanded.
    ROW_EXPANDED,

    // The document was scrolled to an anchor node.
    SCROLLED_TO_ANCHOR,

    // One or more selected children of this node have changed.
    SELECTED_CHILDREN_CHANGED,

    // The text cursor or selection changed.
    SELECTED_TEXT_CHANGED,

    // Text was inserted in a node with text content.
    TEXT_INSERTED,

    // Text was removed in a node with text content.
    TEXT_REMOVED,

    // The node value has changed.
    VALUE_CHANGED,
  };
};

struct ViewHostMsg_RunFileChooser_Mode {
 public:
  enum Value {
    // Requires that the file exists before allowing the user to pick it.
    Open,

    // Like Open, but allows picking multiple files to open.
    OpenMultiple,

    // Like Open, but selects a folder.
    OpenFolder,

    // Allows picking a nonexistent file, and prompts to overwrite if the file
    // already exists.
    Save,
  };
};

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

    // Navigation type not categorized by the other types.
    NORMAL
  };
};

// The user has completed a find-in-page; this type defines what actions the
// renderer should take next.
struct ViewMsg_StopFinding_Params {
  enum Action {
    kClearSelection,
    kKeepSelection,
    kActivateSelection
  };

  ViewMsg_StopFinding_Params() : action(kClearSelection) {}

  // The action that should be taken when the find is completed.
  Action action;
};

#endif  // CONTENT_COMMON_VIEW_MESSAGES_ENUMS_H_
