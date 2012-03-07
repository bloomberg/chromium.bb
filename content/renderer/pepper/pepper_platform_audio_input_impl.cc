// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/pepper/pepper_platform_audio_input_impl.h"

#include "base/bind.h"
#include "base/logging.h"
#include "base/message_loop_proxy.h"
#include "build/build_config.h"
#include "content/common/child_process.h"
#include "content/common/media/audio_messages.h"
#include "content/renderer/render_thread_impl.h"
#include "media/audio/audio_manager_base.h"

PepperPlatformAudioInputImpl::PepperPlatformAudioInputImpl()
    : client_(NULL),
      stream_id_(0),
      main_message_loop_proxy_(base::MessageLoopProxy::current()) {
  filter_ = RenderThreadImpl::current()->audio_input_message_filter();
}

PepperPlatformAudioInputImpl::~PepperPlatformAudioInputImpl() {
  // Make sure we have been shut down. Warning: this will usually happen on
  // the I/O thread!
  DCHECK_EQ(0, stream_id_);
  DCHECK(!client_);
}

// static
PepperPlatformAudioInputImpl* PepperPlatformAudioInputImpl::Create(
    uint32_t sample_rate,
    uint32_t sample_count,
    webkit::ppapi::PluginDelegate::PlatformAudioCommonClient* client) {
  scoped_refptr<PepperPlatformAudioInputImpl> audio_input(
      new PepperPlatformAudioInputImpl);
  if (audio_input->Initialize(sample_rate, sample_count, client)) {
    // Balanced by Release invoked in
    // PepperPlatformAudioInputImpl::ShutDownOnIOThread().
    return audio_input.release();
  }
  return NULL;
}

bool PepperPlatformAudioInputImpl::StartCapture() {
  ChildProcess::current()->io_message_loop()->PostTask(
    FROM_HERE,
    base::Bind(&PepperPlatformAudioInputImpl::StartCaptureOnIOThread, this));
  return true;
}

bool PepperPlatformAudioInputImpl::StopCapture() {
  ChildProcess::current()->io_message_loop()->PostTask(
    FROM_HERE,
    base::Bind(&PepperPlatformAudioInputImpl::StopCaptureOnIOThread, this));
  return true;
}

void PepperPlatformAudioInputImpl::ShutDown() {
  // Called on the main thread to stop all audio callbacks. We must only change
  // the client on the main thread, and the delegates from the I/O thread.
  client_ = NULL;
  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&PepperPlatformAudioInputImpl::ShutDownOnIOThread, this));
}

bool PepperPlatformAudioInputImpl::Initialize(
    uint32_t sample_rate,
    uint32_t sample_count,
    webkit::ppapi::PluginDelegate::PlatformAudioCommonClient* client) {
  DCHECK(client);
  // Make sure we don't call init more than once.
  DCHECK_EQ(0, stream_id_);

  client_ = client;

  AudioParameters params;
  params.format = AudioParameters::AUDIO_PCM_LINEAR;
  params.channels = 1;
  params.sample_rate = sample_rate;
  params.bits_per_sample = 16;
  params.samples_per_packet = sample_count;

  ChildProcess::current()->io_message_loop()->PostTask(
      FROM_HERE,
      base::Bind(&PepperPlatformAudioInputImpl::InitializeOnIOThread,
                 this, params));
  return true;
}

void PepperPlatformAudioInputImpl::InitializeOnIOThread(
    const AudioParameters& params) {
  stream_id_ = filter_->AddDelegate(this);
  filter_->Send(new AudioInputHostMsg_CreateStream(
      stream_id_, params, AudioManagerBase::kDefaultDeviceId));
}

void PepperPlatformAudioInputImpl::StartCaptureOnIOThread() {
  if (stream_id_)
    filter_->Send(new AudioInputHostMsg_RecordStream(stream_id_));
}

void PepperPlatformAudioInputImpl::StopCaptureOnIOThread() {
  if (stream_id_)
    filter_->Send(new AudioInputHostMsg_CloseStream(stream_id_));
}

void PepperPlatformAudioInputImpl::ShutDownOnIOThread() {
  // Make sure we don't call shutdown more than once.
  if (!stream_id_)
    return;

  filter_->Send(new AudioInputHostMsg_CloseStream(stream_id_));
  filter_->RemoveDelegate(stream_id_);
  stream_id_ = 0;

  Release();  // Release for the delegate, balances out the reference taken in
              // PepperPluginDelegateImpl::CreateAudioInput.
}

void PepperPlatformAudioInputImpl::OnStreamCreated(
    base::SharedMemoryHandle handle,
    base::SyncSocket::Handle socket_handle,
    uint32 length) {
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
    main_message_loop_proxy_->PostTask(
        FROM_HERE,
        base::Bind(&PepperPlatformAudioInputImpl::OnStreamCreated, this,
                   handle, socket_handle, length));
  }
}

void PepperPlatformAudioInputImpl::OnVolume(double volume) {
}

void PepperPlatformAudioInputImpl::OnStateChanged(AudioStreamState state) {
}

void PepperPlatformAudioInputImpl::OnDeviceReady(const std::string&) {
}
