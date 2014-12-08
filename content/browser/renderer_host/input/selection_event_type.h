// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SELECTION_EVENT_TYPE_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SELECTION_EVENT_TYPE_

namespace content {

// This file contains a list of events relating to selection and insertion, used
// for notifying Java when the renderer selection has changed.
//
// A Java counterpart will be generated for this enum.
// GENERATED_JAVA_ENUM_PACKAGE: org.chromium.content.browser.input
enum SelectionEventType {
  SELECTION_SHOWN,
  SELECTION_CLEARED,
  SELECTION_DRAG_STARTED,
  SELECTION_DRAG_STOPPED,
  INSERTION_SHOWN,
  INSERTION_MOVED,
  INSERTION_TAPPED,
  INSERTION_CLEARED,
  INSERTION_DRAG_STARTED,
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SELECTION_EVENT_TYPE_
