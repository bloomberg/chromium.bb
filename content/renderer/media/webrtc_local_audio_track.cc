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
      track_source_(track_source),
      need_audio_processing_(!capturer->device_id().empty()) {
  // The capturer with a valid device id is using microphone as source,
  // and APM (AudioProcessingModule) is turned on only for microphone data.
  DCHECK(capturer.get());
  DVLOG(1) << "WebRtcLocalAudioTrack::WebRtcLocalAudioTrack()";

  // TODO(tommi): Remove this, feed audio constraints to WebRtcLocalAudioTrack
  // and check the constraints.  This is here to fix a recent regression whereby
  // audio processing is not enabled for WebAudio regardless of the hard coded
  // audio constraints.  For more info: http://crbug.com/277134
  need_audio_processing_ = true;
}

WebRtcLocalAudioTrack::~WebRtcLocalAudioTrack() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcLocalAudioTrack::~WebRtcLocalAudioTrack()";
  // Users might not call Stop() on the track.
  Stop();
}

void WebRtcLocalAudioTrack::CaptureData(const int16* audio_data,
                                        int number_of_channels,
                                        int number_of_frames,
                                        int audio_delay_milliseconds,
                                        int volume,
                                        bool key_pressed) {
  scoped_refptr<WebRtcAudioCapturer> capturer;
  std::vector<int> voe_channels;
  int sample_rate = 0;
  SinkList sinks;
  {
    base::AutoLock auto_lock(lock_);
    // When the track is diabled, we simply return here.
    // TODO(xians): Figure out if we should feed zero to sinks instead, in
    // order to inject VAD data in such case.
    if (!enabled())
      return;

    capturer = capturer_;
    voe_channels = voe_channels_;
    sample_rate = params_.sample_rate(),
    sinks = sinks_;
  }

  // Feed the data to the sinks.
  for (SinkList::const_iterator it = sinks.begin(); it != sinks.end(); ++it) {
    int new_volume = (*it)->CaptureData(voe_channels,
                                        audio_data,
                                        sample_rate,
                                        number_of_channels,
                                        number_of_frames,
                                        audio_delay_milliseconds,
                                        volume,
                                        need_audio_processing_,
                                        key_pressed);
    if (new_volume != 0 && capturer.get())
      capturer->SetVolume(new_volume);
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

void WebRtcLocalAudioTrack::AddChannel(int channel_id) {
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

void WebRtcLocalAudioTrack::RemoveChannel(int channel_id) {
  DVLOG(1) << "WebRtcLocalAudioTrack::RemoveChannel(channel_id="
           << channel_id << ")";
  base::AutoLock auto_lock(lock_);
  std::vector<int>::iterator iter =
      std::find(voe_channels_.begin(), voe_channels_.end(), channel_id);
  DCHECK(iter != voe_channels_.end());
  voe_channels_.erase(iter);
}

// webrtc::AudioTrackInterface implementation.
webrtc::AudioSourceInterface* WebRtcLocalAudioTrack::GetSource() const {
  return track_source_;
}

cricket::AudioRenderer* WebRtcLocalAudioTrack::GetRenderer() {
  return this;
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
    capturer_->AddTrack(this);
}

void WebRtcLocalAudioTrack::Stop() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(1) << "WebRtcLocalAudioTrack::Stop()";
  if (!capturer_.get())
    return;

  capturer_->RemoveTrack(this);

  // Protect the pointers using the lock when accessing |sinks_| and
  // setting the |capturer_| to NULL.
  SinkList sinks;
  {
    base::AutoLock auto_lock(lock_);
    sinks = sinks_;
    capturer_ = NULL;
  }

  for (SinkList::const_iterator it = sinks.begin(); it != sinks.end(); ++it)
    (*it)->Reset();
}

}  // namespace content
