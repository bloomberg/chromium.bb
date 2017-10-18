// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This is defined here to ensure the D3D11, D3D9, and DXVA includes through
// this header have their GUIDs intialized.
#define INITGUID
#include "media/gpu/d3d11_video_decode_accelerator_win.h"
#undef INITGUID

#include "base/bits.h"
#include "base/memory/ptr_util.h"
#include "base/memory/shared_memory.h"
#include "base/threading/thread_task_runner_handle.h"
#include "base/trace_event/trace_event.h"
#include "media/gpu/d3d11_h264_accelerator.h"
#include "media/gpu/h264_decoder.h"
#include "media/gpu/h264_dpb.h"
#include "ui/gfx/color_space.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_bindings.h"

namespace media {

#define RETURN_ON_FAILURE(result, log, ret) \
  do {                                      \
    if (!(result)) {                        \
      DLOG(ERROR) << log;                   \
      return ret;                           \
    }                                       \
  } while (0)

D3D11VideoDecodeAccelerator::D3D11VideoDecodeAccelerator(
    const GetGLContextCallback& get_gl_context_cb,
    const MakeGLContextCurrentCallback& make_context_current_cb,
    const BindGLImageCallback& bind_image_cb)
    : get_gl_context_cb_(get_gl_context_cb),
      make_context_current_cb_(make_context_current_cb),
      bind_image_cb_(bind_image_cb) {}

D3D11VideoDecodeAccelerator::~D3D11VideoDecodeAccelerator() {}

bool D3D11VideoDecodeAccelerator::Initialize(const Config& config,
                                             Client* client) {
  client_ = client;
  make_context_current_cb_.Run();

  device_ = gl::QueryD3D11DeviceObjectFromANGLE();
  device_->GetImmediateContext(device_context_.GetAddressOf());

  HRESULT hr = device_context_.CopyTo(video_context_.GetAddressOf());
  CHECK(SUCCEEDED(hr));

  hr = device_.CopyTo(video_device_.GetAddressOf());
  CHECK(SUCCEEDED(hr));

  bool is_h264 =
      config.profile >= H264PROFILE_MIN && config.profile <= H264PROFILE_MAX;
  if (!is_h264)
    return false;

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
    CHECK(found);
  }

  D3D11_VIDEO_DECODER_DESC desc = {};
  desc.Guid = decoder_guid;
  desc.SampleWidth = 1920;
  desc.SampleHeight = 1088;
  desc.OutputFormat = DXGI_FORMAT_NV12;
  UINT config_count = 0;
  hr = video_device_->GetVideoDecoderConfigCount(&desc, &config_count);
  if (FAILED(hr) || config_count == 0)
    CHECK(false);

  D3D11_VIDEO_DECODER_CONFIG dec_config = {};
  for (UINT i = 0; i < config_count; i++) {
    hr = video_device_->GetVideoDecoderConfig(&desc, i, &dec_config);
    if (FAILED(hr))
      CHECK(false);
    if (dec_config.ConfigBitstreamRaw == 2)
      break;
  }
  memcpy(&decoder_guid_, &decoder_guid, sizeof decoder_guid_);

  Microsoft::WRL::ComPtr<ID3D11VideoDecoder> video_decoder;
  hr = video_device_->CreateVideoDecoder(&desc, &dec_config,
                                         video_decoder.GetAddressOf());
  CHECK(video_decoder.Get());

  h264_accelerator_.reset(new D3D11H264Accelerator(
      this, video_decoder, video_device_, video_context_));
  decoder_.reset(new media::H264Decoder(h264_accelerator_.get()));

  return true;
}

void D3D11VideoDecodeAccelerator::Decode(
    const BitstreamBuffer& bitstream_buffer) {
  input_buffer_queue_.push_back(bitstream_buffer);
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&D3D11VideoDecodeAccelerator::DoDecode,
                            base::Unretained(this)));
}

void D3D11VideoDecodeAccelerator::DoDecode() {
  if (!bitstream_buffer_) {
    if (input_buffer_queue_.empty())
      return;
    BitstreamBuffer buffer = input_buffer_queue_.front();
    bitstream_buffer_ =
        base::MakeUnique<base::SharedMemory>(buffer.handle(), true);
    bitstream_buffer_->Map(buffer.size());
    bitstream_buffer_size_ = buffer.size();
    input_buffer_id_ = buffer.id();
    input_buffer_queue_.pop_front();
    decoder_->SetStream((const uint8_t*)bitstream_buffer_->memory(),
                        bitstream_buffer_size_);
  }

  while (true) {
    media::AcceleratedVideoDecoder::DecodeResult result = decoder_->Decode();
    if (result == media::AcceleratedVideoDecoder::kRanOutOfStreamData) {
      client_->NotifyEndOfBitstreamBuffer(input_buffer_id_);
      bitstream_buffer_.reset();
      break;
    }
    if (result == media::AcceleratedVideoDecoder::kRanOutOfSurfaces) {
      return;
    }
    if (result == media::AcceleratedVideoDecoder::kAllocateNewSurfaces) {
      client_->ProvidePictureBuffers(20, PIXEL_FORMAT_NV12, 2,
                                     decoder_->GetPicSize(),
                                     GL_TEXTURE_EXTERNAL_OES);
      return;

    } else {
      LOG(ERROR) << "VDA Error " << result;
      CHECK(false);
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&D3D11VideoDecodeAccelerator::DoDecode,
                            base::Unretained(this)));
}

void D3D11VideoDecodeAccelerator::AssignPictureBuffers(
    const std::vector<PictureBuffer>& buffers) {
  D3D11_TEXTURE2D_DESC texture_desc = {};
  texture_desc.Width = decoder_->GetPicSize().width();
  texture_desc.Height = decoder_->GetPicSize().height();
  texture_desc.MipLevels = 1;
  texture_desc.ArraySize = buffers.size();
  texture_desc.Format = DXGI_FORMAT_NV12;
  texture_desc.SampleDesc.Count = 1;
  texture_desc.Usage = D3D11_USAGE_DEFAULT;
  texture_desc.BindFlags = D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE;
  texture_desc.MiscFlags = D3D11_RESOURCE_MISC_SHARED;

  Microsoft::WRL::ComPtr<ID3D11Texture2D> out_texture;
  HRESULT hr = device_->CreateTexture2D(&texture_desc, nullptr,
                                        out_texture.GetAddressOf());
  CHECK(SUCCEEDED(hr));

  make_context_current_cb_.Run();
  picture_buffers_.clear();

  for (size_t i = 0; i < buffers.size(); i++) {
    picture_buffers_.push_back(
        base::MakeUnique<D3D11PictureBuffer>(buffers[i], i));
    picture_buffers_[i]->Init(video_device_, out_texture, decoder_guid_);
    for (uint32_t client_id : buffers[i].client_texture_ids()) {
      // The picture buffer handles the actual binding of its contents to
      // texture ids. This call just causes the texture manager to hold a
      // reference to the GLImage as long as either texture exists.
      bind_image_cb_.Run(client_id, GL_TEXTURE_EXTERNAL_OES,
                         picture_buffers_[i]->gl_image(), true);
    }
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&D3D11VideoDecodeAccelerator::DoDecode,
                            base::Unretained(this)));
}

void D3D11VideoDecodeAccelerator::ReusePictureBuffer(
    int32_t picture_buffer_id) {
  make_context_current_cb_.Run();
  for (auto& buffer : picture_buffers_) {
    if (buffer->picture_buffer().id() == picture_buffer_id) {
      buffer->set_in_client_use(false);

      break;
    }
  }
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE, base::Bind(&D3D11VideoDecodeAccelerator::DoDecode,
                            base::Unretained(this)));
}

D3D11PictureBuffer* D3D11VideoDecodeAccelerator::GetPicture() {
  for (auto& buffer : picture_buffers_)
    if (!buffer->in_client_use() && !buffer->in_picture_use()) {
      return buffer.get();
    }
  return nullptr;
}

void D3D11VideoDecodeAccelerator::Flush() {
  client_->NotifyFlushDone();
}

void D3D11VideoDecodeAccelerator::Reset() {
  client_->NotifyResetDone();
}
void D3D11VideoDecodeAccelerator::Destroy() {}

bool D3D11VideoDecodeAccelerator::TryToSetupDecodeOnSeparateThread(
    const base::WeakPtr<Client>& decode_client,
    const scoped_refptr<base::SingleThreadTaskRunner>& decode_task_runner) {
  return false;
}

GLenum D3D11VideoDecodeAccelerator::GetSurfaceInternalFormat() const {
  return GL_BGRA_EXT;
}

size_t D3D11VideoDecodeAccelerator::input_buffer_id() const {
  return input_buffer_id_;
}

void D3D11VideoDecodeAccelerator::OutputResult(D3D11PictureBuffer* buffer,
                                               size_t input_buffer_id) {
  buffer->set_in_client_use(true);
  Picture picture(buffer->picture_buffer().id(), input_buffer_id,
                  gfx::Rect(0, 0), gfx::ColorSpace(), true);
  client_->PictureReady(picture);
}
}  // namespace media
