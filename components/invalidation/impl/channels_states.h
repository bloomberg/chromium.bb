// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_INVALIDATION_IMPL_CHANNELS_STATES_H_
#define COMPONENTS_INVALIDATION_IMPL_CHANNELS_STATES_H_

namespace syncer {

enum class FcmChannelState {
  NOT_STARTED,
  // Fcm network channel is working properly.
  ENABLED,
  // Failed to retrieve instance id token.
  NO_INSTANCE_ID_TOKEN,

  kMaxValue = NO_INSTANCE_ID_TOKEN,
};

const char* FcmChannelStateToString(FcmChannelState state);

}  // namespace syncer

#endif  // COMPONENTS_INVALIDATION_IMPL_CHANNELS_STATES_H_
