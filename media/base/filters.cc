// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/filters.h"

#include "base/logging.h"

namespace media {

void ResetAndRunCB(FilterStatusCB* cb, PipelineStatus status) {
  DCHECK(!cb->is_null());
  FilterStatusCB tmp_cb(*cb);
  cb->Reset();
  tmp_cb.Run(status);
}

void ResetAndRunCB(base::Closure* cb) {
  DCHECK(!cb->is_null());
  base::Closure tmp_cb(*cb);
  cb->Reset();
  tmp_cb.Run();
}

Filter::Filter() : host_(NULL) {}

Filter::~Filter() {}

void Filter::clear_host() {
  DCHECK(host_);
  host_ = NULL;
}

void Filter::set_host(FilterHost* host) {
  DCHECK(host);
  DCHECK(!host_);
  host_ = host;
}

FilterHost* Filter::host() {
  return host_;
}

void Filter::Play(const base::Closure& callback) {
  DCHECK(!callback.is_null());
  callback.Run();
}

void Filter::Pause(const base::Closure& callback) {
  DCHECK(!callback.is_null());
  callback.Run();
}

void Filter::Flush(const base::Closure& callback) {
  DCHECK(!callback.is_null());
  callback.Run();
}

void Filter::Stop(const base::Closure& callback) {
  DCHECK(!callback.is_null());
  callback.Run();
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

bool VideoDecoder::HasAlpha() const {
  return false;
}

void VideoDecoder::PrepareForShutdownHack() {}

AudioDecoder::AudioDecoder() {}

AudioDecoder::~AudioDecoder() {}

}  // namespace media
