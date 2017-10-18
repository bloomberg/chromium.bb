// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/stream_mixer_input.h"

#include <utility>

#include "chromecast/media/cma/backend/stream_mixer.h"
#include "chromecast/media/cma/backend/stream_mixer_input_impl.h"

namespace chromecast {
namespace media {

StreamMixerInput::StreamMixerInput(Delegate* delegate,
                                   int samples_per_second,
                                   int playout_channel,
                                   bool primary,
                                   const std::string& device_id,
                                   AudioContentType content_type) {
  std::unique_ptr<StreamMixerInputImpl> impl(
      new StreamMixerInputImpl(delegate, samples_per_second, primary, device_id,
                               content_type, StreamMixer::Get()));
  impl_ = impl.get();  // Store a pointer to the impl, but the mixer owns it.
  StreamMixer::Get()->AddInput(std::move(impl));
  StreamMixer::Get()->UpdatePlayoutChannel(playout_channel);
}

StreamMixerInput::~StreamMixerInput() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  impl_->PreventDelegateCalls();
  StreamMixer::Get()->RemoveInput(impl_);
}

void StreamMixerInput::WritePcm(const scoped_refptr<DecoderBufferBase>& data) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  impl_->WritePcm(data);
}

void StreamMixerInput::SetPaused(bool paused) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  impl_->SetPaused(paused);
}

void StreamMixerInput::SetVolumeMultiplier(float multiplier) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  impl_->SetVolumeMultiplier(multiplier);
}

}  // namespace media
}  // namespace chromecast
