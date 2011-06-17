// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_input_device.h"

#include "base/memory/singleton.h"
#include "base/message_loop.h"
#include "content/common/child_process.h"
#include "content/common/media/audio_messages.h"
#include "content/common/view_messages.h"
#include "content/renderer/render_thread.h"
#include "media/audio/audio_util.h"

scoped_refptr<AudioInputMessageFilter> AudioInputDevice::filter_;

namespace {

// AudioMessageFilterCreator is intended to be used as a singleton so we can
// get access to a shared AudioInputMessageFilter.
// Example usage:
//   AudioInputMessageFilter* filter =
//       AudioInputMessageFilterCreator::SharedFilter();

class AudioInputMessageFilterCreator {
 public:
  AudioInputMessageFilterCreator() {
    int routing_id;
    RenderThread::current()->Send(
        new ViewHostMsg_GenerateRoutingID(&routing_id));
    filter_ = new AudioInputMessageFilter(routing_id);
    RenderThread::current()->AddFilter(filter_);
  }

  static AudioInputMessageFilter* SharedFilter() {
    return GetInstance()->filter_.get();
  }

  static AudioInputMessageFilterCreator* GetInstance() {
    return Singleton<AudioInputMessageFilterCreator>::get();
  }

 private:
  scoped_refptr<AudioInputMessageFilter> filter_;
};

}  // namespace

AudioInputDevice::AudioInputDevice(size_t buffer_size,
                                   int channels,
                                   double sample_rate,
                                   CaptureCallback* callback)
    : buffer_size_(buffer_size),
      channels_(channels),
      bits_per_sample_(16),
      sample_rate_(sample_rate),
      callback_(callback),
      audio_delay_milliseconds_(0),
      volume_(1.0),
      stream_id_(0) {
  audio_data_.reserve(channels);
  for (int i = 0; i < channels; ++i) {
    float* channel_data = new float[buffer_size];
    audio_data_.push_back(channel_data);
  }
  // Lazily create the message filter and share across AudioInputDevice
  // instances.
  filter_ = AudioInputMessageFilterCreator::SharedFilter();
}

AudioInputDevice::~AudioInputDevice() {
  // Make sure we have been shut down.
  DCHECK_EQ(0, stream_id_);
  Stop();
  for (int i = 0; i < channels_; ++i)
    delete [] audio_data_[i];
}

bool AudioInputDevice::Start() {
  // Make sure we don't call Start() more than once.
  DCHECK_EQ(0, stream_id_);
  if (stream_id_)
    return false;

  AudioParameters params;
  // TODO(henrika): add support for low-latency mode?
  params.format = AudioParameters::AUDIO_PCM_LINEAR;
  params.channels = channels_;
  params.sample_rate = static_cast<int>(sample_rate_);
  params.bits_per_sample = bits_per_sample_;
  params.samples_per_packet = buffer_size_;

  // Ensure that the initialization task is posted on the I/O thread by
  // accessing the I/O message loop directly. This approach avoids a race
  // condition which could exist if the message loop of the filter was
  // used instead.
  DCHECK(ChildProcess::current()) << "Must be in the renderer";
  MessageLoop* message_loop = ChildProcess::current()->io_message_loop();
  if (!message_loop)
    return false;

  message_loop->PostTask(FROM_HERE,
      NewRunnableMethod(this, &AudioInputDevice::InitializeOnIOThread, params));

  return true;
}

bool AudioInputDevice::Stop() {
  if (!stream_id_)
    return false;

  filter_->message_loop()->PostTask(FROM_HERE,
      NewRunnableMethod(this, &AudioInputDevice::ShutDownOnIOThread));

  if (audio_thread_.get()) {
    socket_->Close();
    audio_thread_->Join();
  }

  return true;
}

bool AudioInputDevice::SetVolume(double volume) {
  NOTIMPLEMENTED();
  return false;
}

bool AudioInputDevice::GetVolume(double* volume) {
  NOTIMPLEMENTED();
  return false;
}

void AudioInputDevice::InitializeOnIOThread(const AudioParameters& params) {
  stream_id_ = filter_->AddDelegate(this);
  filter_->Send(
      new AudioInputHostMsg_CreateStream(0, stream_id_, params, true));
}

void AudioInputDevice::StartOnIOThread() {
  if (stream_id_)
    filter_->Send(new AudioInputHostMsg_RecordStream(0, stream_id_));
}

void AudioInputDevice::ShutDownOnIOThread() {
  // Make sure we don't call shutdown more than once.
  if (!stream_id_)
    return;

  filter_->Send(new AudioInputHostMsg_CloseStream(0, stream_id_));
  filter_->RemoveDelegate(stream_id_);
  stream_id_ = 0;
}

void AudioInputDevice::SetVolumeOnIOThread(double volume) {
  if (stream_id_)
    filter_->Send(new AudioInputHostMsg_SetVolume(0, stream_id_, volume));
}

void AudioInputDevice::OnLowLatencyCreated(
    base::SharedMemoryHandle handle,
    base::SyncSocket::Handle socket_handle,
    uint32 length) {
#if defined(OS_WIN)
  DCHECK(handle);
  DCHECK(socket_handle);
#else
  DCHECK_GE(handle.fd, 0);
  DCHECK_GE(socket_handle, 0);
#endif
  DCHECK(length);

  // TODO(henrika) : check that length is big enough for buffer_size_

  shared_memory_.reset(new base::SharedMemory(handle, false));
  shared_memory_->Map(length);

  socket_.reset(new base::SyncSocket(socket_handle));

  // TODO(henrika): we could optionally set the thread to high-priority
  audio_thread_.reset(
      new base::DelegateSimpleThread(this, "renderer_audio_input_thread"));
  audio_thread_->Start();

  if (filter_) {
    filter_->message_loop()->PostTask(FROM_HERE,
        NewRunnableMethod(this, &AudioInputDevice::StartOnIOThread));
  }
}

void AudioInputDevice::OnVolume(double volume) {
  NOTIMPLEMENTED();
}

// Our audio thread runs here. We receive captured audio samples on
// this thread.
void AudioInputDevice::Run() {
  int pending_data;
  const int samples_per_ms = static_cast<int>(sample_rate_) / 1000;
  const int bytes_per_ms = channels_ * (bits_per_sample_ / 8) * samples_per_ms;

  while (sizeof(pending_data) == socket_->Receive(&pending_data,
                                                  sizeof(pending_data)) &&
                                                  pending_data >= 0) {
    // TODO(henrika): investigate the provided |pending_data| value
    // and ensure that it is actually an accurate delay estimation.

    // Convert the number of pending bytes in the capture buffer
    // into milliseconds.
    audio_delay_milliseconds_ = pending_data / bytes_per_ms;

    FireCaptureCallback();
  }
}

void AudioInputDevice::FireCaptureCallback() {
  if (!callback_)
      return;

  const size_t number_of_frames = buffer_size_;

  // Read 16-bit samples from shared memory (browser writes to it).
  int16* input_audio = static_cast<int16*>(shared_memory_data());
  const int bytes_per_sample = sizeof(input_audio[0]);

  // Deinterleave each channel and convert to 32-bit floating-point
  // with nominal range -1.0 -> +1.0.
  for (int channel_index = 0; channel_index < channels_; ++channel_index) {
    media::DeinterleaveAudioChannel(input_audio,
                                    audio_data_[channel_index],
                                    channels_,
                                    channel_index,
                                    bytes_per_sample,
                                    number_of_frames);
  }

  // Deliver captured data to the client in floating point format
  // and update the audio-delay measurement.
  callback_->Capture(audio_data_,
                     number_of_frames,
                     audio_delay_milliseconds_);
}
