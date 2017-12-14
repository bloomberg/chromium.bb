// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/renderer_host/media/audio_input_stream_handle.h"

#include <utility>

#include "base/bind_helpers.h"
#include "mojo/public/cpp/bindings/interface_request.h"

namespace content {

namespace {

media::mojom::AudioInputStreamClientPtr CreatePtrAndStoreRequest(
    media::mojom::AudioInputStreamClientRequest* request_out) {
  media::mojom::AudioInputStreamClientPtr ptr;
  *request_out = mojo::MakeRequest(&ptr);
  return ptr;
}

}  // namespace

AudioInputStreamHandle::AudioInputStreamHandle(
    mojom::RendererAudioInputStreamFactoryClientPtr client,
    media::MojoAudioInputStream::CreateDelegateCallback
        create_delegate_callback,
    DeleterCallback deleter_callback)
    : deleter_callback_(std::move(deleter_callback)),
      client_(std::move(client)),
      stream_ptr_(),
      stream_client_request_(),
      stream_(mojo::MakeRequest(&stream_ptr_),
              CreatePtrAndStoreRequest(&stream_client_request_),
              std::move(create_delegate_callback),
              base::BindOnce(&AudioInputStreamHandle::OnCreated,
                             base::Unretained(this)),
              base::BindOnce(&AudioInputStreamHandle::CallDeleter,
                             base::Unretained(this))) {
  // Unretained is safe since |this| owns |stream_| and |client_|.
  DCHECK(client_);
  DCHECK(deleter_callback_);
  client_.set_connection_error_handler(base::BindOnce(
      &AudioInputStreamHandle::CallDeleter, base::Unretained(this)));
}

AudioInputStreamHandle::~AudioInputStreamHandle() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
}

void AudioInputStreamHandle::OnCreated(
    mojo::ScopedSharedBufferHandle shared_buffer,
    mojo::ScopedHandle socket_descriptor,
    bool initially_muted) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(client_);
  DCHECK(deleter_callback_)
      << "|deleter_callback_| was called, but |this| hasn't been destructed!";
  client_->StreamCreated(
      std::move(stream_ptr_), std::move(stream_client_request_),
      std::move(shared_buffer), std::move(socket_descriptor), initially_muted);
}

void AudioInputStreamHandle::CallDeleter() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(deleter_callback_);
  std::move(deleter_callback_).Run(this);
}

}  // namespace content
