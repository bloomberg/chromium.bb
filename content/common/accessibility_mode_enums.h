// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_ACCESSIBILITY_MODE_ENUMS_H_
#define CONTENT_COMMON_ACCESSIBILITY_MODE_ENUMS_H_

// Note: keep enums in content/browser/resources/accessibility/accessibility.js
// in sync with these two enums.
enum AccessibilityModeFlag {
  // Native accessibility APIs, specific to each platform, are enabled.
  // When this flag is set that indicates the presence of a third-party
  // client accessing Chrome via accessibility APIs. However, unless one
  // of the flags below is set, the contents of web pages will not be
  // accessible.
  ACCESSIBILITY_MODE_FLAG_NATIVE_APIS = 1 << 0,

  // The renderer process will generate an accessibility tree containing
  // basic information about all nodes, including role, name, value,
  // state, and location. This is the minimum flag required in order for
  // web contents to be accessible, and the remaining flags are meaningless
  // unless this one is set.
  //
  // Note that sometimes this flag will be set when
  // ACCESSIBILITY_MODE_FLAG_NATIVE_APIS is not, when the content layer embedder
  // is providing accessibility support via some other mechanism other than
  // what's implemented in content/browser.
  ACCESSIBILITY_MODE_FLAG_WEB_CONTENTS = 1 << 1,

  // The accessibility tree will contain inline text boxes, which are
  // necessary to expose information about line breaks and word boundaries.
  // Without this flag, you can retrieve the plaintext value of a text field
  // but not the information about how it's broken down into lines.
  //
  // Note that when this flag is off it's still possible to request inline
  // text boxes for a specific node on-demand, asynchronously.
  ACCESSIBILITY_MODE_FLAG_INLINE_TEXT_BOXES = 1 << 2,

  // The accessibility tree will contain extra accessibility
  // attributes typically only needed by screen readers and other
  // assistive technology for blind users. Examples include text style
  // attributes, table cell information, live region properties, range
  // values, and relationship attributes.
  ACCESSIBILITY_MODE_FLAG_SCREEN_READER = 1 << 3,

  // The accessibility tree will contain the HTML tag name and HTML attributes
  // for all accessibility nodes that come from web content.
  ACCESSIBILITY_MODE_FLAG_HTML = 1 << 4,
};

typedef int AccessibilityMode;

const AccessibilityMode AccessibilityModeOff = 0;

const AccessibilityMode ACCESSIBILITY_MODE_COMPLETE =
    ACCESSIBILITY_MODE_FLAG_NATIVE_APIS |
    ACCESSIBILITY_MODE_FLAG_WEB_CONTENTS |
    ACCESSIBILITY_MODE_FLAG_INLINE_TEXT_BOXES |
    ACCESSIBILITY_MODE_FLAG_SCREEN_READER |
    ACCESSIBILITY_MODE_FLAG_HTML;

const AccessibilityMode ACCESSIBILITY_MODE_WEB_CONTENTS_ONLY =
    ACCESSIBILITY_MODE_FLAG_WEB_CONTENTS |
    ACCESSIBILITY_MODE_FLAG_INLINE_TEXT_BOXES |
    ACCESSIBILITY_MODE_FLAG_SCREEN_READER |
    ACCESSIBILITY_MODE_FLAG_HTML;

#endif  // CONTENT_COMMON_ACCESSIBILITY_MODE_ENUMS_H_
