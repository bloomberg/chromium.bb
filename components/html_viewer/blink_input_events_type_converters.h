// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_HTML_VIEWER_BLINK_INPUT_EVENTS_TYPE_CONVERTERS_H_
#define COMPONENTS_HTML_VIEWER_BLINK_INPUT_EVENTS_TYPE_CONVERTERS_H_

#include "base/memory/scoped_ptr.h"
#include "components/mus/public/interfaces/input_events.mojom.h"

namespace blink {
class WebInputEvent;
}

namespace mojo {

template <>
struct TypeConverter<scoped_ptr<blink::WebInputEvent>, mus::mojom::EventPtr> {
  static scoped_ptr<blink::WebInputEvent> Convert(
      const mus::mojom::EventPtr& input);
};

}  // namespace mojo

#endif  // COMPONENTS_HTML_VIEWER_BLINK_INPUT_EVENTS_TYPE_CONVERTERS_H_
