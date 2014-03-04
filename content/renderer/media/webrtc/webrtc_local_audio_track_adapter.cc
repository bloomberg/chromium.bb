// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_local_audio_track_adapter.h"

#include "base/logging.h"
#include "content/renderer/media/webrtc/webrtc_audio_sink_adapter.h"
#include "content/renderer/media/webrtc_local_audio_track.h"

namespace content {

static const char kAudioTrackKind[] = "audio";

scoped_refptr<WebRtcLocalAudioTrackAdapter>
WebRtcLocalAudioTrackAdapter::Create(
    const std::string& label,
    webrtc::AudioSourceInterface* track_source) {
  talk_base::RefCountedObject<WebRtcLocalAudioTrackAdapter>* adapter =
      new talk_base::RefCountedObject<WebRtcLocalAudioTrackAdapter>(
          label, track_source);
  return adapter;
}

WebRtcLocalAudioTrackAdapter::WebRtcLocalAudioTrackAdapter(
    const std::string& label,
    webrtc::AudioSourceInterface* track_source)
    : webrtc::MediaStreamTrack<webrtc::AudioTrackInterface>(label),
      owner_(NULL),
      track_source_(track_source) {
}

WebRtcLocalAudioTrackAdapter::~WebRtcLocalAudioTrackAdapter() {
}

void WebRtcLocalAudioTrackAdapter::Initialize(WebRtcLocalAudioTrack* owner) {
  DCHECK(!owner_);
  DCHECK(owner);
  owner_ = owner;
}

std::string WebRtcLocalAudioTrackAdapter::kind() const {
  return kAudioTrackKind;
}

void WebRtcLocalAudioTrackAdapter::AddSink(
    webrtc::AudioTrackSinkInterface* sink) {
  DCHECK(sink);
#ifndef NDEBUG
  // Verify that |sink| has not been added.
  for (ScopedVector<WebRtcAudioSinkAdapter>::const_iterator it =
           sink_adapters_.begin();
       it != sink_adapters_.end(); ++it) {
    DCHECK(!(*it)->IsEqual(sink));
  }
#endif

  scoped_ptr<WebRtcAudioSinkAdapter> adapter(
      new WebRtcAudioSinkAdapter(sink));
  owner_->AddSink(adapter.get());
  sink_adapters_.push_back(adapter.release());
}

void WebRtcLocalAudioTrackAdapter::RemoveSink(
    webrtc::AudioTrackSinkInterface* sink) {
  DCHECK(sink);
  for (ScopedVector<WebRtcAudioSinkAdapter>::iterator it =
           sink_adapters_.begin();
       it != sink_adapters_.end(); ++it) {
    if ((*it)->IsEqual(sink)) {
      owner_->RemoveSink(*it);
      sink_adapters_.erase(it);
      return;
    }
  }
}

std::vector<int> WebRtcLocalAudioTrackAdapter::VoeChannels() const {
  base::AutoLock auto_lock(lock_);
  return voe_channels_;
}

void WebRtcLocalAudioTrackAdapter::SetSignalLevel(int signal_level) {
  // TODO(xians): Implements this.
}

void WebRtcLocalAudioTrackAdapter::AddChannel(int channel_id) {
  DVLOG(1) << "WebRtcLocalAudioTrack::AddChannel(channel_id="
           << channel_id << ")";
  base::AutoLock auto_lock(lock_);
  if (std::find(voe_channels_.begin(), voe_channels_.end(), channel_id) !=
      voe_channels_.end()) {
    // We need to handle the case when the same channel is connected to the
    // track more than once.
    return;
  }

  voe_channels_.push_back(channel_id);
}

void WebRtcLocalAudioTrackAdapter::RemoveChannel(int channel_id) {
  DVLOG(1) << "WebRtcLocalAudioTrack::RemoveChannel(channel_id="
           << channel_id << ")";
  base::AutoLock auto_lock(lock_);
  std::vector<int>::iterator iter =
      std::find(voe_channels_.begin(), voe_channels_.end(), channel_id);
  DCHECK(iter != voe_channels_.end());
  voe_channels_.erase(iter);
}

webrtc::AudioSourceInterface* WebRtcLocalAudioTrackAdapter::GetSource() const {
  return track_source_;
}

cricket::AudioRenderer* WebRtcLocalAudioTrackAdapter::GetRenderer() {
  return this;
}

}  // namespace content
