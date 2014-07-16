// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_SERVICES_HTML_VIEWER_BLINK_INPUT_EVENTS_TYPE_CONVERTERS_H_
#define MOJO_SERVICES_HTML_VIEWER_BLINK_INPUT_EVENTS_TYPE_CONVERTERS_H_

#include "base/memory/scoped_ptr.h"
#include "mojo/services/public/interfaces/input_events/input_events.mojom.h"
#include "ui/events/event.h"

namespace blink {
class WebInputEvent;
}

namespace mojo {

template<>
class TypeConverter<EventPtr, scoped_ptr<blink::WebInputEvent> > {
 public:
  static scoped_ptr<blink::WebInputEvent> ConvertTo(const EventPtr& input);
};

}  // namespace mojo

#endif  // MOJO_SERVICES_HTML_VIEWER_BLINK_INPUT_EVENTS_TYPE_CONVERTERS_H_
