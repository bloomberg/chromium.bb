// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BLIMP_NET_INPUT_MESSAGE_CONVERTER_H_
#define BLIMP_NET_INPUT_MESSAGE_CONVERTER_H_

#include "base/macros.h"
#include "base/memory/scoped_ptr.h"
#include "blimp/net/blimp_net_export.h"

namespace blink {
class WebGestureEvent;
}

namespace blimp {

class InputMessage;

// Handles creating WebGestureEvents from a stream of InputMessage protos.  This
// class may be stateful to optimize the size of the serialized transmission
// data.  See InputMessageConverter for the deserialize code.
class BLIMP_NET_EXPORT InputMessageConverter {
 public:
  InputMessageConverter();
  ~InputMessageConverter();

  // Process an InputMessage and create a WebGestureEvent from it.  This might
  // make use of state from previous messages.
  scoped_ptr<blink::WebGestureEvent> ProcessMessage(
      const InputMessage& message);

 private:
  DISALLOW_COPY_AND_ASSIGN(InputMessageConverter);
};

}  // namespace blimp

#endif  // BLIMP_NET_INPUT_MESSAGE_CONVERTER_H_
