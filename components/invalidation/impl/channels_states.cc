// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/invalidation/impl/channels_states.h"

namespace syncer {

const char* FcmChannelStateToString(FcmChannelState state) {
  switch (state) {
    case FcmChannelState::NOT_STARTED:
      return "NOT_STARTED";
    case FcmChannelState::ENABLED:
      return "ENABLED";
    case FcmChannelState::NO_INSTANCE_ID_TOKEN:
      return "NO_INSTANCE_ID_TOKEN";
  }
}

}  // namespace syncer
