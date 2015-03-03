// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/audio_state_provider.h"

#include "base/logging.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/public/browser/web_contents.h"

namespace content {

AudioStateProvider::AudioStateProvider(WebContents* contents)
    : web_contents_(contents),
      was_recently_audible_(false) {
  DCHECK(web_contents_);
}

bool AudioStateProvider::WasRecentlyAudible() const {
  return was_recently_audible_;
}

void AudioStateProvider::Notify(bool new_state) {
  if (was_recently_audible_ != new_state) {
    was_recently_audible_ = new_state;
    web_contents_->NotifyNavigationStateChanged(INVALIDATE_TYPE_TAB);
  }
}

} // namespace content
