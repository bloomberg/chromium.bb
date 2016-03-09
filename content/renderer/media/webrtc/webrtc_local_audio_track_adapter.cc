// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc/webrtc_local_audio_track_adapter.h"

#include "base/location.h"
#include "base/logging.h"
#include "content/renderer/media/media_stream_audio_processor.h"
#include "content/renderer/media/webrtc/peer_connection_dependency_factory.h"
#include "content/renderer/media/webrtc/webrtc_audio_sink_adapter.h"
#include "content/renderer/media/webrtc_local_audio_track.h"
#include "content/renderer/render_thread_impl.h"
#include "third_party/webrtc/api/mediastreaminterface.h"

namespace content {

static const char kAudioTrackKind[] = "audio";

scoped_refptr<WebRtcLocalAudioTrackAdapter>
WebRtcLocalAudioTrackAdapter::Create(
    const std::string& label,
    webrtc::AudioSourceInterface* track_source) {
  // TODO(tommi): Change this so that the signaling thread is one of the
  // parameters to this method.
  scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner;
  RenderThreadImpl* current = RenderThreadImpl::current();
  if (current) {
    PeerConnectionDependencyFactory* pc_factory =
        current->GetPeerConnectionDependencyFactory();
    signaling_task_runner = pc_factory->GetWebRtcSignalingThread();
    CHECK(signaling_task_runner);
  } else {
    LOG(WARNING) << "Assuming single-threaded operation for unit test.";
  }

  rtc::RefCountedObject<WebRtcLocalAudioTrackAdapter>* adapter =
      new rtc::RefCountedObject<WebRtcLocalAudioTrackAdapter>(
          label, track_source, std::move(signaling_task_runner));
  return adapter;
}

WebRtcLocalAudioTrackAdapter::WebRtcLocalAudioTrackAdapter(
    const std::string& label,
    webrtc::AudioSourceInterface* track_source,
    scoped_refptr<base::SingleThreadTaskRunner> signaling_task_runner)
    : webrtc::MediaStreamTrack<webrtc::AudioTrackInterface>(label),
      owner_(NULL),
      track_source_(track_source),
      signaling_task_runner_(std::move(signaling_task_runner)) {}

WebRtcLocalAudioTrackAdapter::~WebRtcLocalAudioTrackAdapter() {
}

void WebRtcLocalAudioTrackAdapter::Initialize(WebRtcLocalAudioTrack* owner) {
  DCHECK(!owner_);
  DCHECK(owner);
  owner_ = owner;
}

void WebRtcLocalAudioTrackAdapter::SetAudioProcessor(
    scoped_refptr<MediaStreamAudioProcessor> processor) {
  DCHECK(processor.get());
  DCHECK(!audio_processor_);
  audio_processor_ = std::move(processor);
}

void WebRtcLocalAudioTrackAdapter::SetLevel(
    scoped_refptr<MediaStreamAudioLevelCalculator::Level> level) {
  DCHECK(level.get());
  DCHECK(!level_);
  level_ = std::move(level);
}

std::string WebRtcLocalAudioTrackAdapter::kind() const {
  return kAudioTrackKind;
}

bool WebRtcLocalAudioTrackAdapter::set_enabled(bool enable) {
  // If we're not called on the signaling thread, we need to post a task to
  // change the state on the correct thread.
  if (signaling_task_runner_ &&
      !signaling_task_runner_->BelongsToCurrentThread()) {
    signaling_task_runner_->PostTask(FROM_HERE,
        base::Bind(
            base::IgnoreResult(&WebRtcLocalAudioTrackAdapter::set_enabled),
            this, enable));
    return true;
  }

  return webrtc::MediaStreamTrack<webrtc::AudioTrackInterface>::
      set_enabled(enable);
}

void WebRtcLocalAudioTrackAdapter::AddSink(
    webrtc::AudioTrackSinkInterface* sink) {
  DCHECK(!signaling_task_runner_ ||
         signaling_task_runner_->RunsTasksOnCurrentThread());
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
  DCHECK(!signaling_task_runner_ ||
         signaling_task_runner_->RunsTasksOnCurrentThread());
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

bool WebRtcLocalAudioTrackAdapter::GetSignalLevel(int* level) {
  DCHECK(!signaling_task_runner_ ||
         signaling_task_runner_->RunsTasksOnCurrentThread());

  // |level_| is only set once, so it's safe to read without first acquiring a
  // mutex.
  if (!level_)
    return false;
  const float signal_level = level_->GetCurrent();
  DCHECK_GE(signal_level, 0.0f);
  DCHECK_LE(signal_level, 1.0f);
  // Convert from float in range [0.0,1.0] to an int in range [0,32767].
  *level = static_cast<int>(signal_level * std::numeric_limits<int16_t>::max() +
                            0.5f /* rounding to nearest int */);
  return true;
}

rtc::scoped_refptr<webrtc::AudioProcessorInterface>
WebRtcLocalAudioTrackAdapter::GetAudioProcessor() {
  DCHECK(!signaling_task_runner_ ||
         signaling_task_runner_->RunsTasksOnCurrentThread());
  return audio_processor_.get();
}

webrtc::AudioSourceInterface* WebRtcLocalAudioTrackAdapter::GetSource() const {
  DCHECK(!signaling_task_runner_ ||
         signaling_task_runner_->RunsTasksOnCurrentThread());
  return track_source_;
}

}  // namespace content
