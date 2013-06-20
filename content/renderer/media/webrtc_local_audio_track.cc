// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webrtc_local_audio_track.h"

#include "content/renderer/media/webrtc_audio_capturer.h"
#include "content/renderer/media/webrtc_audio_capturer_sink_owner.h"
#include "third_party/libjingle/source/talk/media/base/audiorenderer.h"

namespace content {

static const char kAudioTrackKind[] = "audio";

scoped_refptr<WebRtcLocalAudioTrack> WebRtcLocalAudioTrack::Create(
    const std::string& id,
    const scoped_refptr<WebRtcAudioCapturer>& capturer,
    webrtc::AudioSourceInterface* track_source) {
  talk_base::RefCountedObject<WebRtcLocalAudioTrack>* track =
      new talk_base::RefCountedObject<WebRtcLocalAudioTrack>(
          id, capturer, track_source);
  return track;
}

WebRtcLocalAudioTrack::WebRtcLocalAudioTrack(
    const std::string& label,
    const scoped_refptr<WebRtcAudioCapturer>& capturer,
    webrtc::AudioSourceInterface* track_source)
    : webrtc::MediaStreamTrack<webrtc::AudioTrackInterface>(label),
      capturer_(capturer),
      track_source_(track_source) {
  DCHECK(capturer.get());
  DVLOG(1) << "WebRtcLocalAudioTrack::WebRtcLocalAudioTrack()";
}

WebRtcLocalAudioTrack::~WebRtcLocalAudioTrack() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(sinks_.empty());
  DVLOG(1) << "WebRtcLocalAudioTrack::~WebRtcLocalAudioTrack()";

  // Users might not call Stop() on the track.
  if (capturer_.get())
    Stop();
}

// Content::WebRtcAudioCapturerSink implementation.
void WebRtcLocalAudioTrack::CaptureData(const int16* audio_data,
                                        int number_of_channels,
                                        int number_of_frames,
                                        int audio_delay_milliseconds,
                                        double volume) {
  SinkList sinks;
  {
    base::AutoLock auto_lock(lock_);
    // When the track is diabled, we simply return here.
    // TODO(xians): Figure out if we should feed zero to sinks instead, in
    // order to inject VAD data in such case.
    if (!enabled())
      return;

    sinks = sinks_;
  }

  // Feed the data to the sinks.
  for (SinkList::const_iterator it = sinks.begin();
       it != sinks.end();
       ++it) {
    (*it)->CaptureData(audio_data, number_of_channels, number_of_frames,
                       audio_delay_milliseconds, volume);
  }
}

void WebRtcLocalAudioTrack::SetCaptureFormat(
    const media::AudioParameters& params) {
  base::AutoLock auto_lock(lock_);
  params_ = params;

  // Update all the existing sinks with the new format.
  for (SinkList::const_iterator it = sinks_.begin();
       it != sinks_.end(); ++it)
    (*it)->SetCaptureFormat(params);
}

// webrtc::AudioTrackInterface implementation.
webrtc::AudioSourceInterface* WebRtcLocalAudioTrack::GetSource() const {
  return track_source_;
}

cricket::AudioRenderer* WebRtcLocalAudioTrack::FrameInput() {
  return NULL;
}

std::string WebRtcLocalAudioTrack::kind() const {
  return kAudioTrackKind;
}

void WebRtcLocalAudioTrack::AddSink(WebRtcAudioCapturerSink* sink) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcLocalAudioTrack::AddSink()";
  base::AutoLock auto_lock(lock_);
  sink->SetCaptureFormat(params_);

  // Verify that |sink| is not already added to the list.
  DCHECK(std::find_if(
      sinks_.begin(), sinks_.end(),
      WebRtcAudioCapturerSinkOwner::WrapsSink(sink)) == sinks_.end());

  // Create (and add to the list) a new WebRtcAudioCapturerSinkOwner which owns
  // the |sink| and delagates all calls to the WebRtcAudioCapturerSink
  // interface.
  sinks_.push_back(new WebRtcAudioCapturerSinkOwner(sink));
}

void WebRtcLocalAudioTrack::RemoveSink(
    WebRtcAudioCapturerSink* sink) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcLocalAudioTrack::RemoveSink()";

  base::AutoLock auto_lock(lock_);

  // Get iterator to the first element for which WrapsSink(sink) returns true.
  SinkList::iterator it = std::find_if(
      sinks_.begin(), sinks_.end(),
      WebRtcAudioCapturerSinkOwner::WrapsSink(sink));
  if (it != sinks_.end()) {
    // Clear the delegate to ensure that no more capture callbacks will
    // be sent to this sink. Also avoids a possible crash which can happen
    // if this method is called while capturing is active.
    (*it)->Reset();
    sinks_.erase(it);
  }
}

void WebRtcLocalAudioTrack::Start() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcLocalAudioTrack::Start()";
  if (capturer_.get())
    capturer_->AddSink(this);
}

void WebRtcLocalAudioTrack::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcLocalAudioTrack::Stop()";
  if (capturer_.get()) {
    capturer_->RemoveSink(this);
    capturer_ = NULL;
  }
}

}  // namespace content
