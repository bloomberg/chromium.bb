// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_COMMON_INPUT_WEB_INPUT_EVENT_TRAITS_H_
#define CONTENT_COMMON_INPUT_WEB_INPUT_EVENT_TRAITS_H_

#include "base/basictypes.h"
#include "content/common/input/scoped_web_input_event.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"

namespace content {

// Utility class for performing operations on and with WebInputEvents.
class WebInputEventTraits {
 public:
  static const char* GetName(WebKit::WebInputEvent::Type type);
  static size_t GetSize(WebKit::WebInputEvent::Type type);
  static ScopedWebInputEvent Clone(const WebKit::WebInputEvent& event);
  static void Delete(WebKit::WebInputEvent* event);
  static bool CanCoalesce(const WebKit::WebInputEvent& event_to_coalesce,
                          const WebKit::WebInputEvent& event);
  static void Coalesce(const WebKit::WebInputEvent& event_to_coalesce,
                       WebKit::WebInputEvent* event);
};

}  // namespace content

#endif  // CONTENT_COMMON_INPUT_WEB_INPUT_EVENT_TRAITS_H_
