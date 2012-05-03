// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/base/filters.h"

#include "base/logging.h"

namespace media {

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

void Filter::Seek(base::TimeDelta time, const PipelineStatusCB& callback) {
  DCHECK(!callback.is_null());
  callback.Run(PIPELINE_OK);
}

void Filter::OnAudioRendererDisabled() {
}

}  // namespace media
