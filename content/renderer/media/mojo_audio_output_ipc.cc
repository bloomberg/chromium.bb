// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/mojo_audio_output_ipc.h"

#include <utility>

#include "media/audio/audio_device_description.h"
#include "media/base/scoped_callback_runner.h"
#include "mojo/public/cpp/system/platform_handle.h"

namespace content {

namespace {

void TrivialAuthorizedCallback(media::OutputDeviceStatus,
                               const media::AudioParameters&,
                               const std::string&) {}

}  // namespace

MojoAudioOutputIPC::MojoAudioOutputIPC(FactoryAccessorCB factory_accessor)
    : factory_accessor_(std::move(factory_accessor)),
      binding_(this),
      weak_factory_(this) {
  DETACH_FROM_THREAD(thread_checker_);
}

MojoAudioOutputIPC::~MojoAudioOutputIPC() {
  DCHECK(!AuthorizationRequested() && !StreamCreationRequested())
      << "CloseStream must be called before destructing the AudioOutputIPC";
  // No thread check.
  // Destructing |weak_factory_| on any thread is safe since it's not used after
  // the final call to CloseStream, where its pointers are invalidated.
}

void MojoAudioOutputIPC::RequestDeviceAuthorization(
    media::AudioOutputIPCDelegate* delegate,
    int session_id,
    const std::string& device_id,
    const url::Origin& security_origin) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegate);
  DCHECK(!delegate_);
  DCHECK(!AuthorizationRequested());
  DCHECK(!StreamCreationRequested());
  delegate_ = delegate;

  // We wrap the callback in a ScopedCallbackRunner to detect the case when the
  // mojo connection is terminated prior to receiving the response. In this
  // case, the callback runner will be destructed and call
  // ReceivedDeviceAuthorization with an error.
  DoRequestDeviceAuthorization(
      session_id, device_id,
      media::ScopedCallbackRunner(
          base::BindOnce(&MojoAudioOutputIPC::ReceivedDeviceAuthorization,
                         weak_factory_.GetWeakPtr()),
          media::OutputDeviceStatus::OUTPUT_DEVICE_STATUS_ERROR_INTERNAL,
          media::AudioParameters::UnavailableDeviceParams(), std::string()));
}

void MojoAudioOutputIPC::CreateStream(media::AudioOutputIPCDelegate* delegate,
                                      const media::AudioParameters& params) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegate);
  DCHECK(!StreamCreationRequested());
  if (!AuthorizationRequested()) {
    DCHECK(!delegate_);
    delegate_ = delegate;
    // No authorization requested yet. Request one for the default device.
    // Since the delegate didn't explicitly request authorization, we shouldn't
    // send a callback to it.
    if (!DoRequestDeviceAuthorization(
            0, media::AudioDeviceDescription::kDefaultDeviceId,
            base::BindOnce(&TrivialAuthorizedCallback))) {
      return;
    }
  }

  DCHECK_EQ(delegate_, delegate);
  // Since the creation callback won't fire if the provider binding is gone
  // and |this| owns |stream_provider_|, unretained is safe.
  media::mojom::AudioOutputStreamClientPtr client_ptr;
  binding_.Bind(mojo::MakeRequest(&client_ptr));
  stream_provider_->Acquire(mojo::MakeRequest(&stream_), std::move(client_ptr),
                            params,
                            base::BindOnce(&MojoAudioOutputIPC::StreamCreated,
                                           base::Unretained(this)));

  // Don't set a connection error handler. Either an error has already been
  // signaled through the AudioOutputStreamClient interface, or the connection
  // is broken because because the frame owning |this| was destroyed, in which
  // case |this| will soon be cleaned up anyways.
}

void MojoAudioOutputIPC::PlayStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (stream_.is_bound())
    stream_->Play();
}

void MojoAudioOutputIPC::PauseStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (stream_.is_bound())
    stream_->Pause();
}

void MojoAudioOutputIPC::CloseStream() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  stream_provider_.reset();
  stream_.reset();
  binding_.Close();
  delegate_ = nullptr;

  // Cancel any pending callbacks for this stream.
  weak_factory_.InvalidateWeakPtrs();
}

void MojoAudioOutputIPC::SetVolume(double volume) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  if (stream_.is_bound())
    stream_->SetVolume(volume);
}

void MojoAudioOutputIPC::OnError() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegate_);
  delegate_->OnError();
}

bool MojoAudioOutputIPC::AuthorizationRequested() {
  return stream_provider_.is_bound();
}

bool MojoAudioOutputIPC::StreamCreationRequested() {
  return stream_.is_bound();
}

media::mojom::AudioOutputStreamProviderRequest
MojoAudioOutputIPC::MakeProviderRequest() {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(!AuthorizationRequested());
  media::mojom::AudioOutputStreamProviderRequest request =
      mojo::MakeRequest(&stream_provider_);

  // Don't set a connection error handler.
  // There are three possible reasons for a connection error.
  // 1. The connection is broken before authorization was completed. In this
  //    case, the ScopedCallbackRunner wrapping the callback will call the
  //    callback with failure.
  // 2. The connection is broken due to authorization being denied. In this
  //    case, the callback was called with failure first, so the state of the
  //    stream provider is irrelevant.
  // 3. The connection was broken after authorization succeeded. This is because
  //    of the frame owning this stream being destructed, and this object will
  //    be cleaned up soon.
  return request;
}

bool MojoAudioOutputIPC::DoRequestDeviceAuthorization(
    int session_id,
    const std::string& device_id,
    AuthorizationCB callback) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  auto* factory = factory_accessor_.Run();
  if (!factory) {
    LOG(ERROR) << "MojoAudioOutputIPC failed to acquire factory";

    // Resetting the callback ensures consistent behaviour with when the factory
    // is destroyed before reply, i.e. calling OnDeviceAuthorized with
    // ERROR_INTERNAL in the normal case. Note that this operation might destroy
    // |this|. The AudioOutputIPCDelegate will call CloseStream as necessary.
    callback.Reset();
    // As |this| may be deleted, no new lines may be added here.
    return false;
  }

  factory->RequestDeviceAuthorization(MakeProviderRequest(), session_id,
                                      device_id, std::move(callback));
  return true;
}

void MojoAudioOutputIPC::ReceivedDeviceAuthorization(
    media::OutputDeviceStatus status,
    const media::AudioParameters& params,
    const std::string& device_id) const {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegate_);
  delegate_->OnDeviceAuthorized(status, params, device_id);
}

void MojoAudioOutputIPC::StreamCreated(
    mojo::ScopedSharedBufferHandle shared_memory,
    mojo::ScopedHandle socket) {
  DCHECK_CALLED_ON_VALID_THREAD(thread_checker_);
  DCHECK(delegate_);
  DCHECK(socket.is_valid());
  DCHECK(shared_memory.is_valid());

  base::PlatformFile socket_handle;
  auto result = mojo::UnwrapPlatformFile(std::move(socket), &socket_handle);
  DCHECK_EQ(result, MOJO_RESULT_OK);

  base::SharedMemoryHandle memory_handle;
  bool read_only = false;
  size_t memory_length = 0;
  result = mojo::UnwrapSharedMemoryHandle(
      std::move(shared_memory), &memory_handle, &memory_length, &read_only);
  DCHECK_EQ(result, MOJO_RESULT_OK);
  DCHECK(!read_only);

  delegate_->OnStreamCreated(memory_handle, socket_handle);
}

}  // namespace content
