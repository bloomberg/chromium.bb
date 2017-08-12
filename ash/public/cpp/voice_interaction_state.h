// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_VOICE_INTERACTION_STATE_H_
#define ASH_PUBLIC_CPP_VOICE_INTERACTION_STATE_H_

namespace ash {

enum class VoiceInteractionState {
  // Voice interaction service is not ready yet, request sent will be waiting.
  NOT_READY = 0,
  // Voice interaction session is stopped.
  STOPPED,
  // Voice interaction session is currently running.
  RUNNING
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_VOICE_INTERACTION_STATE_H_
