// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_video_decoder_impl.h"

#include <d3d11_4.h>

#include "base/threading/sequenced_task_runner_handle.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/gpu/windows/d3d11_picture_buffer.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_bindings.h"

namespace media {

namespace {

static bool MakeContextCurrent(gpu::CommandBufferStub* stub) {
  return stub && stub->decoder()->MakeCurrent();
}

}  // namespace

D3D11VideoDecoderImpl::D3D11VideoDecoderImpl(
    base::RepeatingCallback<gpu::CommandBufferStub*()> get_stub_cb)
    : get_stub_cb_(get_stub_cb), weak_factory_(this) {}

D3D11VideoDecoderImpl::~D3D11VideoDecoderImpl() {
  // TODO(liberato): be sure to clear |picture_buffers_| on the main thread.
  // For now, we always run on the main thread anyway.
}

std::string D3D11VideoDecoderImpl::GetDisplayName() const {
  NOTREACHED() << "Nobody should ask D3D11VideoDecoderImpl for its name";
  return "D3D11VideoDecoderImpl";
}

void D3D11VideoDecoderImpl::Initialize(const VideoDecoderConfig& config,
                                       bool low_delay,
                                       CdmContext* cdm_context,
                                       const InitCB& init_cb,
                                       const OutputCB& output_cb) {
  init_cb_ = init_cb;
  output_cb_ = output_cb;

  stub_ = get_stub_cb_.Run();
  if (!MakeContextCurrent(stub_)) {
    NotifyError("Failed to get decoder stub");
    return;
  }
  // TODO(liberato): see GpuVideoFrameFactory.
  // stub_->AddDestructionObserver(this);

  // Use the ANGLE device, rather than create our own.  It would be nice if we
  // could use our own device, and run on the mojo thread, but texture sharing
  // seems to be difficult.
  device_ = gl::QueryD3D11DeviceObjectFromANGLE();
  device_->GetImmediateContext(device_context_.GetAddressOf());

  HRESULT hr;

  // TODO(liberato): Handle cleanup better.
  hr = device_context_.CopyTo(video_context_.GetAddressOf());
  if (!SUCCEEDED(hr)) {
    NotifyError("Failed to get device context");
    return;
  }

  hr = device_.CopyTo(video_device_.GetAddressOf());
  if (!SUCCEEDED(hr)) {
    NotifyError("Failed to get video device");
    return;
  }

  GUID needed_guid;
  memcpy(&needed_guid, &D3D11_DECODER_PROFILE_H264_VLD_NOFGT,
         sizeof(needed_guid));
  GUID decoder_guid = {};

  {
    // Enumerate supported video profiles and look for the H264 profile.
    bool found = false;
    UINT profile_count = video_device_->GetVideoDecoderProfileCount();
    for (UINT profile_idx = 0; profile_idx < profile_count; profile_idx++) {
      GUID profile_id = {};
      hr = video_device_->GetVideoDecoderProfile(profile_idx, &profile_id);
      if (SUCCEEDED(hr) && (profile_id == needed_guid)) {
        decoder_guid = profile_id;
        found = true;
        break;
      }
    }

    if (!found) {
      NotifyError("Did not find a supported profile");
      return;
    }
  }

  // TODO(liberato): dxva does this.  don't know if we need to.
  Microsoft::WRL::ComPtr<ID3D11Multithread> multi_threaded;
  hr = device_->QueryInterface(IID_PPV_ARGS(&multi_threaded));
  if (!SUCCEEDED(hr)) {
    NotifyError("Failed to query ID3D11Multithread");
    return;
  }
  multi_threaded->SetMultithreadProtected(TRUE);

  D3D11_VIDEO_DECODER_DESC desc = {};
  desc.Guid = decoder_guid;
  // TODO(liberato): where do these numbers come from?
  desc.SampleWidth = 1920;
  desc.SampleHeight = 1088;
  desc.OutputFormat = DXGI_FORMAT_NV12;
  UINT config_count = 0;
  hr = video_device_->GetVideoDecoderConfigCount(&desc, &config_count);
  if (FAILED(hr) || config_count == 0) {
    NotifyError("Failed to get video decoder config count");
    return;
  }

  D3D11_VIDEO_DECODER_CONFIG dec_config = {};
  bool found = false;
  for (UINT i = 0; i < config_count; i++) {
    hr = video_device_->GetVideoDecoderConfig(&desc, i, &dec_config);
    if (FAILED(hr)) {
      NotifyError("Failed to get decoder config");
      return;
    }
    if (dec_config.ConfigBitstreamRaw == 2) {
      found = true;
      break;
    }
  }
  if (!found) {
    NotifyError("Failed to find decoder config");
    return;
  }

  memcpy(&decoder_guid_, &decoder_guid, sizeof decoder_guid_);

  Microsoft::WRL::ComPtr<ID3D11VideoDecoder> video_decoder;
  hr = video_device_->CreateVideoDecoder(&desc, &dec_config,
                                         video_decoder.GetAddressOf());
  if (!video_decoder.Get()) {
    NotifyError("Failed to create a video decoder");
    return;
  }

  h264_accelerator_.reset(new D3D11H264Accelerator(
      this, video_decoder, video_device_, video_context_));
  decoder_.reset(new media::H264Decoder(h264_accelerator_.get()));

  state_ = State::kRunning;
  std::move(init_cb_).Run(true);
}

void D3D11VideoDecoderImpl::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                                   const DecodeCB& decode_cb) {
  if (state_ == State::kError) {
    // TODO(liberato): consider posting, though it likely doesn't matter.
    decode_cb.Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  input_buffer_queue_.push_back(std::make_pair(buffer, decode_cb));
  // Post, since we're not supposed to call back before this returns.  It
  // probably doesn't matter since we're in the gpu process anyway.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&D3D11VideoDecoderImpl::DoDecode, weak_factory_.GetWeakPtr()));
}

void D3D11VideoDecoderImpl::DoDecode() {
  if (state_ != State::kRunning)
    return;

  if (!current_buffer_) {
    if (input_buffer_queue_.empty()) {
      return;
    }
    current_buffer_ = input_buffer_queue_.front().first;
    current_decode_cb_ = input_buffer_queue_.front().second;
    current_timestamp_ = current_buffer_->timestamp();
    input_buffer_queue_.pop_front();
    if (current_buffer_->end_of_stream()) {
      // Flush, then signal the decode cb once all pictures have been output.
      current_buffer_ = nullptr;
      if (!decoder_->Flush()) {
        // This will also signal error |current_decode_cb_|.
        NotifyError("Flush failed");
        return;
      }
      // Pictures out output synchronously during Flush.  Signal the decode
      // cb now.
      std::move(current_decode_cb_).Run(DecodeStatus::OK);
      return;
    }
    decoder_->SetStream((const uint8_t*)current_buffer_->data(),
                        current_buffer_->data_size());
  }

  while (true) {
    // If we transition to the error state, then stop here.
    if (state_ == State::kError)
      return;

    media::AcceleratedVideoDecoder::DecodeResult result = decoder_->Decode();
    // TODO(liberato): switch + class enum.
    if (result == media::AcceleratedVideoDecoder::kRanOutOfStreamData) {
      current_buffer_ = nullptr;
      std::move(current_decode_cb_).Run(DecodeStatus::OK);
      break;
    } else if (result == media::AcceleratedVideoDecoder::kRanOutOfSurfaces) {
      // At this point, we know the picture size.
      // If we haven't allocated picture buffers yet, then allocate some now.
      // Otherwise, stop here.  We'll restart when a picture comes back.
      if (picture_buffers_.size())
        return;
      CreatePictureBuffers();
    } else if (result == media::AcceleratedVideoDecoder::kAllocateNewSurfaces) {
      CreatePictureBuffers();
    } else {
      LOG(ERROR) << "VDA Error " << result;
      NotifyError("Accelerated decode failed");
      return;
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&D3D11VideoDecoderImpl::DoDecode, weak_factory_.GetWeakPtr()));
}

void D3D11VideoDecoderImpl::Reset(const base::Closure& closure) {
  current_buffer_ = nullptr;
  if (current_decode_cb_)
    std::move(current_decode_cb_).Run(DecodeStatus::ABORTED);

  for (auto& queue_pair : input_buffer_queue_)
    queue_pair.second.Run(DecodeStatus::ABORTED);
  input_buffer_queue_.clear();

  // TODO(liberato): how do we signal an error?
  decoder_->Reset();
  closure.Run();
}

bool D3D11VideoDecoderImpl::NeedsBitstreamConversion() const {
  // This is called from multiple threads.
  return true;
}

bool D3D11VideoDecoderImpl::CanReadWithoutStalling() const {
  // This is called from multiple threads.
  return false;
}

int D3D11VideoDecoderImpl::GetMaxDecodeRequests() const {
  // This is called from multiple threads.
  return 4;
}

void D3D11VideoDecoderImpl::CreatePictureBuffers() {
  // TODO(liberato): what's the minimum that we need for the decoder?
  // the VDA requests 20.
  const int num_buffers = 20;

  gfx::Size size = decoder_->GetPicSize();

  // Create an array of |num_buffers| elements to back the PictureBuffers.
  D3D11_TEXTURE2D_DESC texture_desc = {};
  texture_desc.Width = size.width();
  texture_desc.Height = size.height();
  texture_desc.MipLevels = 1;
  texture_desc.ArraySize = num_buffers;
  texture_desc.Format = DXGI_FORMAT_NV12;
  texture_desc.SampleDesc.Count = 1;
  texture_desc.Usage = D3D11_USAGE_DEFAULT;
  texture_desc.BindFlags = D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE;
  texture_desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> out_texture;
  HRESULT hr = device_->CreateTexture2D(&texture_desc, nullptr,
                                        out_texture.GetAddressOf());
  if (!SUCCEEDED(hr)) {
    NotifyError("Failed to create a Texture2D for PictureBuffers");
    return;
  }

  // Drop any old pictures.
  for (auto& buffer : picture_buffers_)
    DCHECK(!buffer->in_picture_use());
  picture_buffers_.clear();

  // Create each picture buffer.
  const int textures_per_picture = 2;  // From the VDA
  for (size_t i = 0; i < num_buffers; i++) {
    picture_buffers_.push_back(
        new D3D11PictureBuffer(GL_TEXTURE_EXTERNAL_OES, size, i));
    if (!picture_buffers_[i]->Init(get_stub_cb_, video_device_, out_texture,
                                   decoder_guid_, textures_per_picture)) {
      NotifyError("Unable to allocate PictureBuffer");
      return;
    }
  }
}

D3D11PictureBuffer* D3D11VideoDecoderImpl::GetPicture() {
  for (auto& buffer : picture_buffers_) {
    if (!buffer->in_client_use() && !buffer->in_picture_use()) {
      buffer->timestamp_ = current_timestamp_;
      return buffer.get();
    }
  }

  return nullptr;
}

void D3D11VideoDecoderImpl::OutputResult(D3D11PictureBuffer* buffer) {
  buffer->set_in_client_use(true);

  // Note: The pixel format doesn't matter.
  gfx::Rect visible_rect(buffer->size());
  gfx::Size natural_size = buffer->size();
  base::TimeDelta timestamp = buffer->timestamp_;
  auto frame = VideoFrame::WrapNativeTextures(
      PIXEL_FORMAT_NV12, buffer->mailbox_holders(),
      VideoFrame::ReleaseMailboxCB(), buffer->size(), visible_rect,
      natural_size, timestamp);

  frame->SetReleaseMailboxCB(media::BindToCurrentLoop(base::BindOnce(
      &D3D11VideoDecoderImpl::OnMailboxReleased, weak_factory_.GetWeakPtr(),
      scoped_refptr<D3D11PictureBuffer>(buffer))));
  output_cb_.Run(frame);
}

void D3D11VideoDecoderImpl::OnMailboxReleased(
    scoped_refptr<D3D11PictureBuffer> buffer,
    const gpu::SyncToken& sync_token) {
  // Note that |buffer| might no longer be in |picture_buffers_| if we've
  // replaced them.  That's okay.

  // TODO(liberato): what about the sync token?
  buffer->set_in_client_use(false);

  // Also re-start decoding in case it was waiting for more pictures.
  // TODO(liberato): there might be something pending already.  we should
  // probably check.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&D3D11VideoDecoderImpl::DoDecode, weak_factory_.GetWeakPtr()));
}

base::WeakPtr<D3D11VideoDecoderImpl> D3D11VideoDecoderImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

void D3D11VideoDecoderImpl::NotifyError(const char* reason) {
  state_ = State::kError;
  DLOG(ERROR) << reason;
  if (init_cb_)
    std::move(init_cb_).Run(false);

  if (current_decode_cb_)
    std::move(current_decode_cb_).Run(DecodeStatus::DECODE_ERROR);

  for (auto& queue_pair : input_buffer_queue_)
    queue_pair.second.Run(DecodeStatus::DECODE_ERROR);
}

}  // namespace media
