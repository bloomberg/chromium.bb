// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/mojo/services/mojo_audio_input_stream.h"

#include <memory>
#include <utility>

#include "base/callback_helpers.h"
#include "base/memory/shared_memory.h"
#include "base/sync_socket.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace media {

MojoAudioInputStream::MojoAudioInputStream(
    mojom::AudioInputStreamRequest request,
    mojom::AudioInputStreamClientPtr client,
    CreateDelegateCallback create_delegate_callback,
    StreamCreatedCallback stream_created_callback,
    base::OnceClosure deleter_callback)
    : stream_created_callback_(std::move(stream_created_callback)),
      deleter_callback_(std::move(deleter_callback)),
      binding_(this, std::move(request)),
      client_(std::move(client)),
      weak_factory_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stream_created_callback_);
  DCHECK(deleter_callback_);
  // |this| owns |binding_|, so unretained is safe.
  binding_.set_connection_error_handler(
      base::BindOnce(&MojoAudioInputStream::OnError, base::Unretained(this)));
  client_.set_connection_error_handler(
      base::BindOnce(&MojoAudioInputStream::OnError, base::Unretained(this)));
  delegate_ = std::move(create_delegate_callback).Run(this);
  if (!delegate_) {
    // Failed to initialize the stream. We cannot call |deleter_callback_| yet,
    // since construction isn't done.
    binding_.Close();
    base::ThreadTaskRunnerHandle::Get()->PostTask(
        FROM_HERE,
        base::BindOnce(&MojoAudioInputStream::OnStreamError,
                       weak_factory_.GetWeakPtr(), /* not used */ 0));
  }
}

MojoAudioInputStream::~MojoAudioInputStream() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void MojoAudioInputStream::Record() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  delegate_->OnRecordStream();
}

void MojoAudioInputStream::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (volume < 0 || volume > 1) {
    LOG(ERROR) << "MojoAudioInputStream::SetVolume(" << volume
               << ") out of range.";
    OnStreamError(/*not used*/ 0);
    return;
  }
  delegate_->OnSetVolume(volume);
}

void MojoAudioInputStream::OnStreamCreated(
    int stream_id,
    const base::SharedMemory* shared_memory,
    std::unique_ptr<base::CancelableSyncSocket> foreign_socket,
    bool initially_muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(stream_created_callback_);
  DCHECK(shared_memory);
  DCHECK(foreign_socket);

  base::SharedMemoryHandle foreign_memory_handle =
      shared_memory->GetReadOnlyHandle();
  if (!base::SharedMemory::IsHandleValid(foreign_memory_handle)) {
    OnStreamError(/*not used*/ 0);
    return;
  }

  mojo::ScopedSharedBufferHandle buffer_handle = mojo::WrapSharedMemoryHandle(
      foreign_memory_handle, shared_memory->requested_size(),
      mojo::UnwrappedSharedMemoryHandleProtection::kReadOnly);
  mojo::ScopedHandle socket_handle =
      mojo::WrapPlatformFile(foreign_socket->Release());

  DCHECK(buffer_handle.is_valid());
  DCHECK(socket_handle.is_valid());

  base::ResetAndReturn(&stream_created_callback_)
      .Run(std::move(buffer_handle), std::move(socket_handle), initially_muted);
}

void MojoAudioInputStream::OnMuted(int stream_id, bool is_muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_->OnMutedStateChanged(is_muted);
}

void MojoAudioInputStream::OnStreamError(int stream_id) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  client_->OnError();
  OnError();
}

void MojoAudioInputStream::OnError() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(deleter_callback_);
  std::move(deleter_callback_).Run();  // Deletes |this|.
}

}  // namespace media
