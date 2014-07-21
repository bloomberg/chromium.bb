// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_RENDERER_HOST_INPUT_SELECTION_EVENT_TYPE_
#define CONTENT_BROWSER_RENDERER_HOST_INPUT_SELECTION_EVENT_TYPE_

namespace content {

enum SelectionEventType {
#define DEFINE_SELECTION_EVENT_TYPE(name, value) name = value,
#include "content/browser/renderer_host/input/selection_event_type_list.h"
#undef DEFINE_SELECTION_EVENT_TYPE
};

}  // namespace content

#endif  // CONTENT_BROWSER_RENDERER_HOST_INPUT_SELECTION_EVENT_TYPE_
