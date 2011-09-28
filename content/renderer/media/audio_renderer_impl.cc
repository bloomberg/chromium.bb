// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/audio_renderer_impl.h"

#include <math.h>

#include <algorithm>

#include "base/command_line.h"
#include "content/common/child_process.h"
#include "content/common/content_switches.h"
#include "content/common/media/audio_messages.h"
#include "content/renderer/render_thread.h"
#include "content/renderer/render_view.h"
#include "media/audio/audio_buffers_state.h"
#include "media/audio/audio_output_controller.h"
#include "media/audio/audio_util.h"
#include "media/base/filter_host.h"

// Static variable that says what code path we are using -- low or high
// latency. Made separate variable so we don't have to go to command line
// for every DCHECK().
AudioRendererImpl::LatencyType AudioRendererImpl::latency_type_ =
    AudioRendererImpl::kUninitializedLatency;

AudioRendererImpl::AudioRendererImpl()
    : AudioRendererBase(),
      bytes_per_second_(0),
      stream_id_(0),
      shared_memory_(NULL),
      shared_memory_size_(0),
      stopped_(false),
      pending_request_(false),
      prerolling_(false),
      preroll_bytes_(0) {
  filter_ = RenderThread::current()->audio_message_filter();
  // Figure out if we are planning to use high or low latency code path.
  // We are initializing only one variable and double initialization is Ok,
  // so there would not be any issues caused by CPU memory model.
  if (latency_type_ == kUninitializedLatency) {
    if (!CommandLine::ForCurrentProcess()->HasSwitch(
        switches::kHighLatencyAudio)) {
      latency_type_ = kLowLatency;
    } else {
      latency_type_ = kHighLatency;
    }
  }
}

AudioRendererImpl::~AudioRendererImpl() {
}

// static
void AudioRendererImpl::set_latency_type(LatencyType latency_type) {
  DCHECK_EQ(kUninitializedLatency, latency_type_);
  latency_type_ = latency_type;
}

base::TimeDelta AudioRendererImpl::ConvertToDuration(int bytes) {
  if (bytes_per_second_) {
    return base::TimeDelta::FromMicroseconds(
        base::Time::kMicrosecondsPerSecond * bytes / bytes_per_second_);
  }
  return base::TimeDelta();
}

void AudioRendererImpl::UpdateEarliestEndTime(int bytes_filled,
                                              base::TimeDelta request_delay,
                                              base::Time time_now) {
  if (bytes_filled != 0) {
    base::TimeDelta predicted_play_time = ConvertToDuration(bytes_filled);
    float playback_rate = GetPlaybackRate();
    if (playback_rate != 1.0f) {
      predicted_play_time = base::TimeDelta::FromMicroseconds(
          static_cast<int64>(ceil(predicted_play_time.InMicroseconds() *
                                  playback_rate)));
    }
    earliest_end_time_ =
        std::max(earliest_end_time_,
                 time_now + request_delay + predicted_play_time);
  }
}

bool AudioRendererImpl::OnInitialize(int bits_per_channel,
                                     ChannelLayout channel_layout,
                                     int sample_rate) {
  AudioParameters params(AudioParameters::AUDIO_PCM_LINEAR, channel_layout,
                         sample_rate, bits_per_channel, 0);

  bytes_per_second_ = params.GetBytesPerSecond();

  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioRendererImpl::CreateStreamTask, params));
  return true;
}

void AudioRendererImpl::OnStop() {
  base::AutoLock auto_lock(lock_);
  if (stopped_)
    return;
  stopped_ = true;

  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioRendererImpl::DestroyTask));

  if (audio_thread_.get()) {
    socket_->Close();
    audio_thread_->Join();
  }
}

void AudioRendererImpl::NotifyDataAvailableIfNecessary() {
  if (latency_type_ == kHighLatency) {
    // Post a task to render thread to notify a packet reception.
    ChildProcess::current()->io_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &AudioRendererImpl::NotifyPacketReadyTask));
  }
}

void AudioRendererImpl::ConsumeAudioSamples(
    scoped_refptr<media::Buffer> buffer_in) {
  base::AutoLock auto_lock(lock_);
  if (stopped_)
    return;

  // TODO(hclam): handle end of stream here.

  // Use the base class to queue the buffer.
  AudioRendererBase::ConsumeAudioSamples(buffer_in);

  NotifyDataAvailableIfNecessary();
}

void AudioRendererImpl::SetPlaybackRate(float rate) {
  DCHECK_LE(0.0f, rate);

  base::AutoLock auto_lock(lock_);
  // Handle the case where we stopped due to IO message loop dying.
  if (stopped_) {
    AudioRendererBase::SetPlaybackRate(rate);
    return;
  }

  // We have two cases here:
  // Play: GetPlaybackRate() == 0.0 && rate != 0.0
  // Pause: GetPlaybackRate() != 0.0 && rate == 0.0
  if (GetPlaybackRate() == 0.0f && rate != 0.0f) {
    ChildProcess::current()->io_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &AudioRendererImpl::PlayTask));
  } else if (GetPlaybackRate() != 0.0f && rate == 0.0f) {
    // Pause is easy, we can always pause.
    ChildProcess::current()->io_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &AudioRendererImpl::PauseTask));
  }
  AudioRendererBase::SetPlaybackRate(rate);

  // If we are playing, give a kick to try fulfilling the packet request as
  // the previous packet request may be stalled by a pause.
  if (rate > 0.0f) {
    NotifyDataAvailableIfNecessary();
  }
}

void AudioRendererImpl::Pause(media::FilterCallback* callback) {
  AudioRendererBase::Pause(callback);
  base::AutoLock auto_lock(lock_);
  if (stopped_)
    return;

  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioRendererImpl::PauseTask));
}

void AudioRendererImpl::Seek(base::TimeDelta time,
                             const media::FilterStatusCB& cb) {
  AudioRendererBase::Seek(time, cb);
  base::AutoLock auto_lock(lock_);
  if (stopped_)
    return;

  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioRendererImpl::SeekTask));
}


void AudioRendererImpl::Play(media::FilterCallback* callback) {
  AudioRendererBase::Play(callback);
  base::AutoLock auto_lock(lock_);
  if (stopped_)
    return;

  if (GetPlaybackRate() != 0.0f) {
    ChildProcess::current()->io_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &AudioRendererImpl::PlayTask));
  } else {
    ChildProcess::current()->io_message_loop()->PostTask(
        FROM_HERE,
        NewRunnableMethod(this, &AudioRendererImpl::PauseTask));
  }
}

void AudioRendererImpl::SetVolume(float volume) {
  base::AutoLock auto_lock(lock_);
  if (stopped_)
    return;
  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      NewRunnableMethod(this, &AudioRendererImpl::SetVolumeTask, volume));
}

void AudioRendererImpl::OnCreated(base::SharedMemoryHandle handle,
                                  uint32 length) {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());
  DCHECK_EQ(kHighLatency, latency_type_);

  base::AutoLock auto_lock(lock_);
  if (stopped_)
    return;

  shared_memory_.reset(new base::SharedMemory(handle, false));
  shared_memory_->Map(length);
  shared_memory_size_ = length;
}

void AudioRendererImpl::CreateSocket(base::SyncSocket::Handle socket_handle) {
  DCHECK_EQ(kLowLatency, latency_type_);
#if defined(OS_WIN)
  DCHECK(socket_handle);
#else
  DCHECK_GE(socket_handle, 0);
#endif
  socket_.reset(new base::SyncSocket(socket_handle));
}

void AudioRendererImpl::CreateAudioThread() {
  DCHECK_EQ(kLowLatency, latency_type_);
  audio_thread_.reset(
      new base::DelegateSimpleThread(this, "renderer_audio_thread"));
  audio_thread_->Start();
}

void AudioRendererImpl::OnLowLatencyCreated(
    base::SharedMemoryHandle handle,
    base::SyncSocket::Handle socket_handle,
    uint32 length) {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());
  DCHECK_EQ(kLowLatency, latency_type_);
#if defined(OS_WIN)
  DCHECK(handle);
#else
  DCHECK_GE(handle.fd, 0);
#endif
  DCHECK_NE(0u, length);

  base::AutoLock auto_lock(lock_);
  if (stopped_)
    return;

  shared_memory_.reset(new base::SharedMemory(handle, false));
  shared_memory_->Map(media::TotalSharedMemorySizeInBytes(length));
  shared_memory_size_ = length;

  CreateSocket(socket_handle);
  CreateAudioThread();
}

void AudioRendererImpl::OnRequestPacket(AudioBuffersState buffers_state) {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());
  DCHECK_EQ(kHighLatency, latency_type_);
  {
    base::AutoLock auto_lock(lock_);
    DCHECK(!pending_request_);
    pending_request_ = true;
    request_buffers_state_ = buffers_state;
  }

  // Try to fill in the fulfill the packet request.
  NotifyPacketReadyTask();
}

void AudioRendererImpl::OnStateChanged(AudioStreamState state) {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());

  base::AutoLock auto_lock(lock_);
  if (stopped_)
    return;

  switch (state) {
    case kAudioStreamError:
      // We receive this error if we counter an hardware error on the browser
      // side. We can proceed with ignoring the audio stream.
      // TODO(hclam): We need more handling of these kind of error. For example
      // re-try creating the audio output stream on the browser side or fail
      // nicely and report to demuxer that the whole audio stream is discarded.
      host()->DisableAudioRenderer();
      break;
    // TODO(hclam): handle these events.
    case kAudioStreamPlaying:
    case kAudioStreamPaused:
      break;
    default:
      NOTREACHED();
      break;
  }
}

void AudioRendererImpl::OnVolume(double volume) {
  // TODO(hclam): decide whether we need to report the current volume to
  // pipeline.
}

void AudioRendererImpl::CreateStreamTask(const AudioParameters& audio_params) {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());

  base::AutoLock auto_lock(lock_);
  if (stopped_)
    return;

  // Make sure we don't call create more than once.
  DCHECK_EQ(0, stream_id_);
  stream_id_ = filter_->AddDelegate(this);
  ChildProcess::current()->io_message_loop()->AddDestructionObserver(this);

  AudioParameters params_to_send(audio_params);
  // Let the browser choose packet size.
  params_to_send.samples_per_packet = 0;

  Send(new AudioHostMsg_CreateStream(stream_id_,
                                     params_to_send,
                                     latency_type_ == kLowLatency));
}

void AudioRendererImpl::PlayTask() {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());

  earliest_end_time_ = base::Time::Now();
  Send(new AudioHostMsg_PlayStream(stream_id_));
}

void AudioRendererImpl::PauseTask() {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());

  Send(new AudioHostMsg_PauseStream(stream_id_));
}

void AudioRendererImpl::SeekTask() {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());

  earliest_end_time_ = base::Time::Now();
  // We have to pause the audio stream before we can flush.
  Send(new AudioHostMsg_PauseStream(stream_id_));
  Send(new AudioHostMsg_FlushStream(stream_id_));
}

void AudioRendererImpl::DestroyTask() {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());

  // Make sure we don't call destroy more than once.
  DCHECK_NE(0, stream_id_);
  filter_->RemoveDelegate(stream_id_);
  Send(new AudioHostMsg_CloseStream(stream_id_));
  // During shutdown this may be NULL; don't worry about deregistering in that
  // case.
  if (ChildProcess::current())
    ChildProcess::current()->io_message_loop()->RemoveDestructionObserver(this);
  stream_id_ = 0;
}

void AudioRendererImpl::SetVolumeTask(double volume) {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());

  base::AutoLock auto_lock(lock_);
  if (stopped_)
    return;
  Send(new AudioHostMsg_SetVolume(stream_id_, volume));
}

void AudioRendererImpl::NotifyPacketReadyTask() {
  DCHECK(MessageLoop::current() == ChildProcess::current()->io_message_loop());
  DCHECK_EQ(kHighLatency, latency_type_);

  base::AutoLock auto_lock(lock_);
  if (stopped_)
    return;
  if (pending_request_ && GetPlaybackRate() > 0.0f) {
    DCHECK(shared_memory_.get());

    // Adjust the playback delay.
    base::Time current_time = base::Time::Now();

    base::TimeDelta request_delay =
        ConvertToDuration(request_buffers_state_.total_bytes());

    // Add message delivery delay.
    if (current_time > request_buffers_state_.timestamp) {
      base::TimeDelta receive_latency =
          current_time - request_buffers_state_.timestamp;

      // If the receive latency is too much it may offset all the delay.
      if (receive_latency >= request_delay) {
        request_delay = base::TimeDelta();
      } else {
        request_delay -= receive_latency;
      }
    }

    // Finally we need to adjust the delay according to playback rate.
    if (GetPlaybackRate() != 1.0f) {
      request_delay = base::TimeDelta::FromMicroseconds(
          static_cast<int64>(ceil(request_delay.InMicroseconds() *
                                  GetPlaybackRate())));
    }

    bool buffer_empty = (request_buffers_state_.pending_bytes == 0) &&
                        (current_time >= earliest_end_time_);

    // For high latency mode we don't write length into shared memory,
    // it is explicit part of AudioHostMsg_NotifyPacketReady() message,
    // so no need to reserve first word of buffer for length.
    uint32 filled = FillBuffer(static_cast<uint8*>(shared_memory_->memory()),
                               shared_memory_size_, request_delay,
                               buffer_empty);
    UpdateEarliestEndTime(filled, request_delay, current_time);
    pending_request_ = false;

    // Then tell browser process we are done filling into the buffer.
    Send(new AudioHostMsg_NotifyPacketReady(stream_id_, filled));
  }
}

void AudioRendererImpl::WillDestroyCurrentMessageLoop() {
  DCHECK(!ChildProcess::current() ||  // During shutdown.
         (MessageLoop::current() ==
          ChildProcess::current()->io_message_loop()));

  // We treat the IO loop going away the same as stopping.
  base::AutoLock auto_lock(lock_);
  if (stopped_)
    return;

  stopped_ = true;
  DestroyTask();
}

// Our audio thread runs here. We receive requests for more data and send it
// on this thread.
void AudioRendererImpl::Run() {
  audio_thread_->SetThreadPriority(base::kThreadPriority_RealtimeAudio);

  int bytes;
  while (sizeof(bytes) == socket_->Receive(&bytes, sizeof(bytes))) {
    if (bytes == media::AudioOutputController::kPauseMark)
      continue;
    else if (bytes < 0)
      break;
    base::AutoLock auto_lock(lock_);
    if (stopped_)
      break;
    float playback_rate = GetPlaybackRate();
    if (playback_rate <= 0.0f)
      continue;
    DCHECK(shared_memory_.get());
    base::TimeDelta request_delay = ConvertToDuration(bytes);

    // We need to adjust the delay according to playback rate.
    if (playback_rate != 1.0f) {
      request_delay = base::TimeDelta::FromMicroseconds(
          static_cast<int64>(ceil(request_delay.InMicroseconds() *
                                  playback_rate)));
    }
    base::Time time_now = base::Time::Now();
    uint32 size = FillBuffer(static_cast<uint8*>(shared_memory_->memory()),
                             shared_memory_size_,
                             request_delay,
                             time_now >= earliest_end_time_);
    media::SetActualDataSizeInBytes(shared_memory_.get(),
                                    shared_memory_size_,
                                    size);
    UpdateEarliestEndTime(size, request_delay, time_now);
  }
}

void AudioRendererImpl::Send(IPC::Message* message) {
  filter_->Send(message);
}
