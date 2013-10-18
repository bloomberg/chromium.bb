// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/audio/audio_output_dispatcher.h"

#include "base/message_loop/message_loop_proxy.h"

namespace media {

AudioOutputDispatcher::AudioOutputDispatcher(
    AudioManager* audio_manager,
    const AudioParameters& params,
    const std::string& output_device_id,
    const std::string& input_device_id)
    : audio_manager_(audio_manager),
      message_loop_(audio_manager->GetMessageLoop()),
      params_(params),
      output_device_id_(output_device_id),
      input_device_id_(input_device_id) {
  // We expect to be instantiated on the audio thread.  Otherwise the
  // message_loop_ member will point to the wrong message loop!
  DCHECK(audio_manager->GetMessageLoop()->BelongsToCurrentThread());
}

AudioOutputDispatcher::~AudioOutputDispatcher() {
  DCHECK(message_loop_->BelongsToCurrentThread());
}

}  // namespace media
