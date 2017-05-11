// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromecast/media/cma/backend/android/audio_sink_android.h"

#include <string>
#include <utility>

#include "chromecast/media/cma/backend/android/audio_sink_android_audiotrack_impl.h"
#include "chromecast/media/cma/base/decoder_buffer_base.h"

namespace chromecast {
namespace media {

AudioSinkAndroid::AudioSinkAndroid(Delegate* delegate,
                                   int samples_per_second,
                                   bool primary,
                                   const std::string& device_id,
                                   AudioContentType content_type) {
  impl_.reset(new AudioSinkAndroidAudioTrackImpl(
      delegate, samples_per_second, primary, device_id, content_type));
}

AudioSinkAndroid::~AudioSinkAndroid() {
  DCHECK(thread_checker_.CalledOnValidThread());
  impl_->PreventDelegateCalls();
}

void AudioSinkAndroid::WritePcm(const scoped_refptr<DecoderBufferBase>& data) {
  DCHECK(thread_checker_.CalledOnValidThread());
  impl_->WritePcm(data);
}

void AudioSinkAndroid::SetPaused(bool paused) {
  DCHECK(thread_checker_.CalledOnValidThread());
  impl_->SetPaused(paused);
}

void AudioSinkAndroid::SetVolumeMultiplier(float multiplier) {
  DCHECK(thread_checker_.CalledOnValidThread());
  impl_->SetVolumeMultiplier(multiplier);
}

}  // namespace media
}  // namespace chromecast
