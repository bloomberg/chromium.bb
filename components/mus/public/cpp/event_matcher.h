// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_EVENT_MATCHER_H_
#define COMPONENTS_MUS_PUBLIC_CPP_EVENT_MATCHER_H_

#include "ui/mojo/events/input_event_constants.mojom.h"
#include "ui/mojo/events/input_event_matcher.mojom.h"
#include "ui/mojo/events/input_key_codes.mojom.h"

namespace mus {

mojo::EventMatcherPtr CreateKeyMatcher(mojo::KeyboardCode code,
                                       mojo::EventFlags flags);

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_EVENT_MATCHER_H_
