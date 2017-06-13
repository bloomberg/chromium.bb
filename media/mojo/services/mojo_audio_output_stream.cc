// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_audio_output_stream.h"

#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/memory/shared_memory.h"
#include "base/sync_socket.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace media {

MojoAudioOutputStream::MojoAudioOutputStream(
    mojom::AudioOutputStreamRequest request,
    CreateDelegateCallback create_delegate_callback,
    StreamCreatedCallback stream_created_callback,
    base::OnceClosure deleter_callback)
    : stream_created_callback_(std::move(stream_created_callback)),
      deleter_callback_(std::move(deleter_callback)),
      binding_(this, std::move(request)),
      weak_factory_(this) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(deleter_callback_);
  // |this| owns |binding_|, so unretained is safe.
  binding_.set_connection_error_handler(
      base::Bind(&MojoAudioOutputStream::OnError, base::Unretained(this)));
  delegate_ = std::move(create_delegate_callback).Run(this);
  if (!delegate_) {
    // Failed to initialize the stream. We cannot call |deleter_callback_| yet,
    // since construction isn't done.
    binding_.Close();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE, base::Bind(&MojoAudioOutputStream::OnError,
                              weak_factory_.GetWeakPtr()));
  }
}

MojoAudioOutputStream::~MojoAudioOutputStream() {
  DCHECK(thread_checker_.CalledOnValidThread());
}

void MojoAudioOutputStream::Play() {
  DCHECK(thread_checker_.CalledOnValidThread());
  delegate_->OnPlayStream();
}

void MojoAudioOutputStream::Pause() {
  DCHECK(thread_checker_.CalledOnValidThread());
  delegate_->OnPauseStream();
}

void MojoAudioOutputStream::SetVolume(double volume) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (volume < 0 || volume > 1) {
    LOG(ERROR) << "MojoAudioOutputStream::SetVolume(" << volume
               << ") out of range.";
    OnError();
    return;
  }
  delegate_->OnSetVolume(volume);
}

void MojoAudioOutputStream::OnStreamCreated(
    int stream_id,
    const base::SharedMemory* shared_memory,
    std::unique_ptr<base::CancelableSyncSocket> foreign_socket) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(stream_created_callback_);
  DCHECK(shared_memory);
  DCHECK(foreign_socket);

  base::SharedMemoryHandle foreign_memory_handle =
      base::SharedMemory::DuplicateHandle(shared_memory->handle());
  DCHECK(base::SharedMemory::IsHandleValid(foreign_memory_handle));

  mojo::ScopedSharedBufferHandle buffer_handle = mojo::WrapSharedMemoryHandle(
      foreign_memory_handle, shared_memory->requested_size(), false);
  mojo::ScopedHandle socket_handle =
      mojo::WrapPlatformFile(foreign_socket->Release());

  DCHECK(buffer_handle.is_valid());
  DCHECK(socket_handle.is_valid());

  base::ResetAndReturn(&stream_created_callback_)
      .Run(std::move(buffer_handle), std::move(socket_handle));
}

void MojoAudioOutputStream::OnStreamError(int stream_id) {
  DCHECK(thread_checker_.CalledOnValidThread());
  OnError();
}

void MojoAudioOutputStream::OnError() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(deleter_callback_);
  std::move(deleter_callback_).Run();  // Deletes |this|.
}

}  // namespace media
