// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef MOJO_CONVERTERS_BLINK_BLINK_INPUT_EVENTS_TYPE_CONVERTERS_H_
#define MOJO_CONVERTERS_BLINK_BLINK_INPUT_EVENTS_TYPE_CONVERTERS_H_

#include <memory>

#include "components/mus/public/interfaces/input_events.mojom.h"
#include "mojo/converters/blink/mojo_blink_export.h"

namespace blink {
class WebInputEvent;
}

namespace mojo {

template <>
struct MOJO_BLINK_EXPORT
    TypeConverter<std::unique_ptr<blink::WebInputEvent>, mus::mojom::EventPtr> {
  static std::unique_ptr<blink::WebInputEvent> Convert(
      const mus::mojom::EventPtr& input);
};

}  // namespace mojo

#endif  // MOJO_CONVERTERS_BLINK_BLINK_INPUT_EVENTS_TYPE_CONVERTERS_H_
