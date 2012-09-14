// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_platform_audio_output_impl.h"

#include "base/bind.h"
#include "base/command_line.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "build/build_config.h"
#include "content/common/child_process.h"
#include "content/common/media/audio_messages.h"
#include "content/renderer/media/audio_hardware.h"
#include "content/renderer/media/audio_message_filter.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/media_switches.h"

namespace content {

// static
PepperPlatformAudioOutputImpl* PepperPlatformAudioOutputImpl::Create(
    int sample_rate,
    int frames_per_buffer,
    webkit::ppapi::PluginDelegate::PlatformAudioOutputClient* client) {
  scoped_refptr<PepperPlatformAudioOutputImpl> audio_output(
      new PepperPlatformAudioOutputImpl());
  if (audio_output->Initialize(sample_rate, frames_per_buffer, client)) {
    // Balanced by Release invoked in
    // PepperPlatformAudioOutputImpl::ShutDownOnIOThread().
    return audio_output.release();
  }
  return NULL;
}

bool PepperPlatformAudioOutputImpl::StartPlayback() {
  if (ipc_) {
    ChildProcess::current()->io_message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&PepperPlatformAudioOutputImpl::StartPlaybackOnIOThread,
                   this));
    return true;
  }
  return false;
}

bool PepperPlatformAudioOutputImpl::StopPlayback() {
  if (ipc_) {
    ChildProcess::current()->io_message_loop()->PostTask(
        FROM_HERE,
        base::Bind(&PepperPlatformAudioOutputImpl::StopPlaybackOnIOThread,
                   this));
    return true;
  }
  return false;
}

void PepperPlatformAudioOutputImpl::ShutDown() {
  // Called on the main thread to stop all audio callbacks. We must only change
  // the client on the main thread, and the delegates from the I/O thread.
  client_ = NULL;
  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&PepperPlatformAudioOutputImpl::ShutDownOnIOThread, this));
}

void PepperPlatformAudioOutputImpl::OnStateChanged(
    media::AudioOutputIPCDelegate::State state) {
}

void PepperPlatformAudioOutputImpl::OnStreamCreated(
    base::SharedMemoryHandle handle,
    base::SyncSocket::Handle socket_handle,
    int length) {
#if defined(OS_WIN)
  DCHECK(handle);
  DCHECK(socket_handle);
#else
  DCHECK_NE(-1, handle.fd);
  DCHECK_NE(-1, socket_handle);
#endif
  DCHECK(length);

  if (base::MessageLoopProxy::current() == main_message_loop_proxy_) {
    // Must dereference the client only on the main thread. Shutdown may have
    // occurred while the request was in-flight, so we need to NULL check.
    if (client_)
      client_->StreamCreated(handle, length, socket_handle);
  } else {
    main_message_loop_proxy_->PostTask(FROM_HERE,
        base::Bind(&PepperPlatformAudioOutputImpl::OnStreamCreated, this,
                   handle, socket_handle, length));
  }
}

void PepperPlatformAudioOutputImpl::OnIPCClosed() {
  ipc_ = NULL;
}

PepperPlatformAudioOutputImpl::~PepperPlatformAudioOutputImpl() {
  // Make sure we have been shut down. Warning: this will usually happen on
  // the I/O thread!
  DCHECK_EQ(0, stream_id_);
  DCHECK(!client_);
}

PepperPlatformAudioOutputImpl::PepperPlatformAudioOutputImpl()
    : client_(NULL),
      stream_id_(0),
      main_message_loop_proxy_(base::MessageLoopProxy::current()) {
  ipc_ = RenderThreadImpl::current()->audio_message_filter();
}

bool PepperPlatformAudioOutputImpl::Initialize(
    int sample_rate,
    int frames_per_buffer,
    webkit::ppapi::PluginDelegate::PlatformAudioOutputClient* client) {
  DCHECK(client);
  // Make sure we don't call init more than once.
  DCHECK_EQ(0, stream_id_);

  client_ = client;

  media::AudioParameters::Format format;
  const int kMaxFramesForLowLatency = 2047;

  const CommandLine* cmd_line = CommandLine::ForCurrentProcess();
  if (!cmd_line->HasSwitch(switches::kDisableAudioOutputResampler)) {
    // Rely on AudioOutputResampler to handle any inconsistencies between the
    // hardware params required for low latency and the requested params.
    format = media::AudioParameters::AUDIO_PCM_LOW_LATENCY;
  } else if (sample_rate == audio_hardware::GetOutputSampleRate() &&
             frames_per_buffer <= kMaxFramesForLowLatency &&
             frames_per_buffer % audio_hardware::GetOutputBufferSize() == 0) {
    // Use the low latency back end if the client request is compatible, and
    // the sample count is low enough to justify using AUDIO_PCM_LOW_LATENCY.
    format = media::AudioParameters::AUDIO_PCM_LOW_LATENCY;
  } else {
    format = media::AudioParameters::AUDIO_PCM_LINEAR;
  }

  media::AudioParameters params(format, CHANNEL_LAYOUT_STEREO, sample_rate, 16,
                                frames_per_buffer);

  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&PepperPlatformAudioOutputImpl::InitializeOnIOThread,
                 this, params));
  return true;
}

void PepperPlatformAudioOutputImpl::InitializeOnIOThread(
    const media::AudioParameters& params) {
  stream_id_ = ipc_->AddDelegate(this);
  ipc_->CreateStream(stream_id_, params, 0);
}

void PepperPlatformAudioOutputImpl::StartPlaybackOnIOThread() {
  if (stream_id_)
    ipc_->PlayStream(stream_id_);
}

void PepperPlatformAudioOutputImpl::StopPlaybackOnIOThread() {
  if (stream_id_)
    ipc_->PauseStream(stream_id_);
}

void PepperPlatformAudioOutputImpl::ShutDownOnIOThread() {
  // Make sure we don't call shutdown more than once.
  if (!stream_id_)
    return;

  ipc_->CloseStream(stream_id_);
  ipc_->RemoveDelegate(stream_id_);
  stream_id_ = 0;

  Release();  // Release for the delegate, balances out the reference taken in
              // PepperPluginDelegateImpl::CreateAudio.
}

}  // namespace content
