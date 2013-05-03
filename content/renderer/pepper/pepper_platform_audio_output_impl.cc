// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_platform_audio_output_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "build/build_config.h"
#include "content/common/child_process.h"
#include "content/common/media/audio_messages.h"
#include "content/renderer/media/audio_message_filter.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/audio_hardware_config.h"

namespace content {

// static
PepperPlatformAudioOutputImpl* PepperPlatformAudioOutputImpl::Create(
    int sample_rate,
    int frames_per_buffer,
    int source_render_view_id,
    webkit::ppapi::PluginDelegate::PlatformAudioOutputClient* client) {
  scoped_refptr<PepperPlatformAudioOutputImpl> audio_output(
      new PepperPlatformAudioOutputImpl());
  if (audio_output->Initialize(sample_rate, frames_per_buffer,
                               source_render_view_id, client)) {
    // Balanced by Release invoked in
    // PepperPlatformAudioOutputImpl::ShutDownOnIOThread().
    audio_output->AddRef();
    return audio_output.get();
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
  ipc_.reset();
}

PepperPlatformAudioOutputImpl::~PepperPlatformAudioOutputImpl() {
  // Make sure we have been shut down. Warning: this will usually happen on
  // the I/O thread!
  DCHECK(!ipc_);
  DCHECK(!client_);
}

PepperPlatformAudioOutputImpl::PepperPlatformAudioOutputImpl()
    : client_(NULL),
      main_message_loop_proxy_(base::MessageLoopProxy::current()) {
}

bool PepperPlatformAudioOutputImpl::Initialize(
    int sample_rate,
    int frames_per_buffer,
    int source_render_view_id,
    webkit::ppapi::PluginDelegate::PlatformAudioOutputClient* client) {
  DCHECK(client);
  client_ = client;

  RenderThreadImpl* const render_thread = RenderThreadImpl::current();
  ipc_ = render_thread->audio_message_filter()->
      CreateAudioOutputIPC(source_render_view_id);
  CHECK(ipc_);

  media::AudioParameters params(
      media::AudioParameters::AUDIO_PCM_LOW_LATENCY,
      media::CHANNEL_LAYOUT_STEREO, sample_rate, 16, frames_per_buffer);

  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&PepperPlatformAudioOutputImpl::InitializeOnIOThread,
                 this, params));
  return true;
}

void PepperPlatformAudioOutputImpl::InitializeOnIOThread(
    const media::AudioParameters& params) {
  DCHECK(ChildProcess::current()->io_message_loop_proxy()->
      BelongsToCurrentThread());
  if (ipc_)
    ipc_->CreateStream(this, params);
}

void PepperPlatformAudioOutputImpl::StartPlaybackOnIOThread() {
  DCHECK(ChildProcess::current()->io_message_loop_proxy()->
      BelongsToCurrentThread());
  if (ipc_)
    ipc_->PlayStream();
}

void PepperPlatformAudioOutputImpl::StopPlaybackOnIOThread() {
  DCHECK(ChildProcess::current()->io_message_loop_proxy()->
      BelongsToCurrentThread());
  if (ipc_)
    ipc_->PauseStream();
}

void PepperPlatformAudioOutputImpl::ShutDownOnIOThread() {
  DCHECK(ChildProcess::current()->io_message_loop_proxy()->
      BelongsToCurrentThread());

  // Make sure we don't call shutdown more than once.
  if (!ipc_)
    return;

  ipc_->CloseStream();
  ipc_.reset();

  Release();  // Release for the delegate, balances out the reference taken in
              // PepperPluginDelegateImpl::CreateAudio.
}

}  // namespace content
