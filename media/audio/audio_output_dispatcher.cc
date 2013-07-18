// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_dispatcher.h"

#include "base/message_loop/message_loop.h"

namespace media {

AudioOutputDispatcher::AudioOutputDispatcher(
    AudioManager* audio_manager,
    const AudioParameters& params,
    const std::string& input_device_id)
    : audio_manager_(audio_manager),
      message_loop_(base::MessageLoop::current()),
      params_(params),
      input_device_id_(input_device_id) {
  // We expect to be instantiated on the audio thread.  Otherwise the
  // message_loop_ member will point to the wrong message loop!
  DCHECK(audio_manager->GetMessageLoop()->BelongsToCurrentThread());
}

AudioOutputDispatcher::~AudioOutputDispatcher() {
  DCHECK_EQ(base::MessageLoop::current(), message_loop_);
}

}  // namespace media
