// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_INPUT_EVENT_DISPATCH_TYPE_H_
#define CONTENT_COMMON_INPUT_INPUT_EVENT_DISPATCH_TYPE_H_

namespace content {

enum InputEventDispatchType {
  DISPATCH_TYPE_NORMAL,        // Dispatch a normal event.
  DISPATCH_TYPE_NON_BLOCKING,  // Dispatch a non-blocking event.
  DISPATCH_TYPE_MAX = DISPATCH_TYPE_NON_BLOCKING
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_INPUT_EVENT_DISPATCH_TYPE_H_
