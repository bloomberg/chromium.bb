// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/filters.h"

#include "base/logging.h"

namespace media {

void ResetAndRunCB(FilterStatusCB* cb, PipelineStatus status) {
  DCHECK(cb);
  FilterStatusCB tmp_cb(*cb);
  cb->Reset();
  tmp_cb.Run(status);
}

Filter::Filter() : host_(NULL) {}

Filter::~Filter() {}

void Filter::set_host(FilterHost* host) {
  DCHECK(host);
  DCHECK(!host_);
  host_ = host;
}

FilterHost* Filter::host() {
  return host_;
}

void Filter::Play(FilterCallback* callback) {
  DCHECK(callback);
  callback->Run();
  delete callback;
}

void Filter::Pause(FilterCallback* callback) {
  DCHECK(callback);
  callback->Run();
  delete callback;
}

void Filter::Flush(FilterCallback* callback) {
  DCHECK(callback);
  callback->Run();
  delete callback;
}

void Filter::Stop(FilterCallback* callback) {
  DCHECK(callback);
  callback->Run();
  delete callback;
}

void Filter::SetPlaybackRate(float playback_rate) {}

void Filter::Seek(base::TimeDelta time, const FilterStatusCB& callback) {
  DCHECK(!callback.is_null());
  callback.Run(PIPELINE_OK);
}

void Filter::OnAudioRendererDisabled() {
}

VideoDecoder::VideoDecoder() {}

VideoDecoder::~VideoDecoder() {}

AudioDecoder::AudioDecoder() {}

AudioDecoder::~AudioDecoder() {}

void AudioDecoder::ConsumeAudioSamples(scoped_refptr<Buffer> buffer) {
  consume_audio_samples_callback_.Run(buffer);
}

}  // namespace media
