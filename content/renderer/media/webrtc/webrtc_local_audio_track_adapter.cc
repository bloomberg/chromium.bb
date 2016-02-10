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
  scoped_refptr<base::SingleThreadTaskRunner> signaling_thread;
  RenderThreadImpl* current = RenderThreadImpl::current();
  if (current) {
    PeerConnectionDependencyFactory* pc_factory =
        current->GetPeerConnectionDependencyFactory();
    signaling_thread = pc_factory->GetWebRtcSignalingThread();
  }

  LOG_IF(ERROR, !signaling_thread.get()) << "No signaling thread!";

  rtc::RefCountedObject<WebRtcLocalAudioTrackAdapter>* adapter =
      new rtc::RefCountedObject<WebRtcLocalAudioTrackAdapter>(
          label, track_source, signaling_thread);
  return adapter;
}

WebRtcLocalAudioTrackAdapter::WebRtcLocalAudioTrackAdapter(
    const std::string& label,
    webrtc::AudioSourceInterface* track_source,
    const scoped_refptr<base::SingleThreadTaskRunner>& signaling_thread)
    : webrtc::MediaStreamTrack<webrtc::AudioTrackInterface>(label),
      owner_(NULL),
      track_source_(track_source),
      signaling_thread_(signaling_thread),
      signal_level_(0) {
  signaling_thread_checker_.DetachFromThread();
  capture_thread_.DetachFromThread();
}

WebRtcLocalAudioTrackAdapter::~WebRtcLocalAudioTrackAdapter() {
}

void WebRtcLocalAudioTrackAdapter::Initialize(WebRtcLocalAudioTrack* owner) {
  DCHECK(!owner_);
  DCHECK(owner);
  owner_ = owner;
}

void WebRtcLocalAudioTrackAdapter::SetAudioProcessor(
    const scoped_refptr<MediaStreamAudioProcessor>& processor) {
  // SetAudioProcessor will be called when a new capture thread has been
  // initialized, so we need to detach from any current capture thread we're
  // checking and attach to the current one.
  capture_thread_.DetachFromThread();
  DCHECK(capture_thread_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  audio_processor_ = processor;
}

std::string WebRtcLocalAudioTrackAdapter::kind() const {
  return kAudioTrackKind;
}

bool WebRtcLocalAudioTrackAdapter::set_enabled(bool enable) {
  // If we're not called on the signaling thread, we need to post a task to
  // change the state on the correct thread.
  if (signaling_thread_.get() && !signaling_thread_->BelongsToCurrentThread()) {
    signaling_thread_->PostTask(FROM_HERE,
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
  DCHECK(signaling_thread_checker_.CalledOnValidThread());
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
  DCHECK(signaling_thread_checker_.CalledOnValidThread());
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
  DCHECK(signaling_thread_checker_.CalledOnValidThread());

  base::AutoLock auto_lock(lock_);
  *level = signal_level_;
  return true;
}

rtc::scoped_refptr<webrtc::AudioProcessorInterface>
WebRtcLocalAudioTrackAdapter::GetAudioProcessor() {
  DCHECK(signaling_thread_checker_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  return audio_processor_.get();
}

void WebRtcLocalAudioTrackAdapter::SetSignalLevel(int signal_level) {
  DCHECK(capture_thread_.CalledOnValidThread());
  base::AutoLock auto_lock(lock_);
  signal_level_ = signal_level;
}

webrtc::AudioSourceInterface* WebRtcLocalAudioTrackAdapter::GetSource() const {
  DCHECK(signaling_thread_checker_.CalledOnValidThread());
  return track_source_;
}

}  // namespace content
