// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_MUS_PUBLIC_CPP_EVENT_MATCHER_UTIL_H_
#define COMPONENTS_MUS_PUBLIC_CPP_EVENT_MATCHER_UTIL_H_

#include "components/mus/public/interfaces/event_matcher.mojom.h"
#include "components/mus/public/interfaces/input_event_constants.mojom.h"
#include "components/mus/public/interfaces/input_key_codes.mojom.h"

namespace mus {

// |flags| is a bitfield of kEventFlag* and kMouseEventFlag* values in
// input_event_constants.mojom.
mojom::EventMatcherPtr CreateKeyMatcher(mojom::KeyboardCode code, int flags);

}  // namespace mus

#endif  // COMPONENTS_MUS_PUBLIC_CPP_EVENT_MATCHER_UTIL_H_
