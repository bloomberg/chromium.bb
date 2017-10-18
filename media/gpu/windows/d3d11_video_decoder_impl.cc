// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_video_decoder_impl.h"

#include <d3d11_4.h>

#include "base/threading/sequenced_task_runner_handle.h"
#include "gpu/command_buffer/service/mailbox_manager.h"
#include "gpu/command_buffer/service/texture_manager.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_bindings.h"

namespace media {

namespace {

static bool MakeContextCurrent(gpu::GpuCommandBufferStub* stub) {
  return stub && stub->decoder()->MakeCurrent();
}

}  // namespace

D3D11VideoDecoderImpl::D3D11VideoDecoderImpl(
    base::Callback<gpu::GpuCommandBufferStub*()> get_stub_cb,
    OutputWithReleaseMailboxCB output_cb)
    : get_stub_cb_(get_stub_cb),
      output_cb_(std::move(output_cb)),
      weak_factory_(this) {}

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
                                       const OutputCB& /* output_cb */) {
  stub_ = get_stub_cb_.Run();
  if (!MakeContextCurrent(stub_)) {
    init_cb.Run(false);
    return;
  }
  // TODO(liberato): see GpuVideoFrameFactory.
  // stub_->AddDestructionObserver(this);
  decoder_helper_ = GLES2DecoderHelper::Create(stub_->decoder());

  // The device is threadsafe.  Does GetImmediateContext create a new object?
  // If so, then we can just use it on the MCVD thread.  All we need to do is
  // to share the Texture object, somehow, which is likely trivial.  We'd have
  // to set up the picturebuffers on the main thread.
  // We might need to do the query on the main thread, too, since it probably
  // needs the context to be current.
  device_ = gl::QueryD3D11DeviceObjectFromANGLE();
  device_->GetImmediateContext(device_context_.GetAddressOf());

  // TODO(liberato): Handle cleanup better.
  HRESULT hr = device_context_.CopyTo(video_context_.GetAddressOf());
  if (!SUCCEEDED(hr)) {
    init_cb.Run(false);
    return;
  }

  hr = device_.CopyTo(video_device_.GetAddressOf());
  if (!SUCCEEDED(hr)) {
    init_cb.Run(false);
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
      init_cb.Run(false);
      return;
    }
  }

  // TODO(liberato): dxva does this.  don't know if we need to.
  base::win::ScopedComPtr<ID3D11Multithread> multi_threaded;
  hr = device_->QueryInterface(IID_PPV_ARGS(&multi_threaded));
  if (!SUCCEEDED(hr)) {
    init_cb.Run(false);
  }
  multi_threaded->SetMultithreadProtected(TRUE);

  D3D11_VIDEO_DECODER_DESC desc = {};
  desc.Guid = decoder_guid;
  desc.SampleWidth = 1920;
  desc.SampleHeight = 1088;
  desc.OutputFormat = DXGI_FORMAT_NV12;
  UINT config_count = 0;
  hr = video_device_->GetVideoDecoderConfigCount(&desc, &config_count);
  if (FAILED(hr) || config_count == 0) {
    init_cb.Run(false);
    return;
  }

  D3D11_VIDEO_DECODER_CONFIG dec_config = {};
  for (UINT i = 0; i < config_count; i++) {
    hr = video_device_->GetVideoDecoderConfig(&desc, i, &dec_config);
    if (FAILED(hr)) {
      init_cb.Run(false);
      return;
    }
    // TODO(liberato): what happens if we don't break on any iteration?
    // crbug.com/775577 .
    if (dec_config.ConfigBitstreamRaw == 2)
      break;
  }
  memcpy(&decoder_guid_, &decoder_guid, sizeof decoder_guid_);

  base::win::ScopedComPtr<ID3D11VideoDecoder> video_decoder;
  hr = video_device_->CreateVideoDecoder(&desc, &dec_config,
                                         video_decoder.GetAddressOf());
  if (!video_decoder.Get()) {
    init_cb.Run(false);
    return;
  }

  h264_accelerator_.reset(new D3D11H264Accelerator(
      this, video_decoder, video_device_, video_context_));
  decoder_.reset(new media::H264Decoder(h264_accelerator_.get()));

  init_cb.Run(true);
}

void D3D11VideoDecoderImpl::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                                   const DecodeCB& decode_cb) {
  input_buffer_queue_.push_back(std::make_pair(buffer, decode_cb));
  // TODO(liberato): Why post?
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&D3D11VideoDecoderImpl::DoDecode, weak_factory_.GetWeakPtr()));
}

void D3D11VideoDecoderImpl::DoDecode() {
  if (!current_buffer_) {
    if (input_buffer_queue_.empty()) {
      return;
    }
    current_buffer_ = input_buffer_queue_.front().first;
    current_decode_cb_ = input_buffer_queue_.front().second;
    current_timestamp_ = current_buffer_->timestamp();
    input_buffer_queue_.pop_front();
    if (current_buffer_->end_of_stream()) {
      // TODO(liberato): flush, then signal the decode cb once all pictures
      // have been output.  For now, we approximate this as "do nothing".
      current_buffer_ = nullptr;
      return;
    }
    decoder_->SetStream((const uint8_t*)current_buffer_->data(),
                        current_buffer_->data_size());
  }

  while (true) {
    media::AcceleratedVideoDecoder::DecodeResult result = decoder_->Decode();
    if (result == media::AcceleratedVideoDecoder::kRanOutOfStreamData) {
      current_buffer_ = nullptr;
      std::move(current_decode_cb_).Run(DecodeStatus::OK);
      break;
    } else if (result == media::AcceleratedVideoDecoder::kRanOutOfSurfaces) {
      // At this point, we know the picture size.
      // If we haven't allocated picture buffers yet, then allocate some now.
      // Otherwise, stop here.  We'll restart when a picture comes back.
      if (!picture_buffers_.size())
        CreatePictureBuffers();
      else
        return;
    } else if (result == media::AcceleratedVideoDecoder::kAllocateNewSurfaces) {
      CreatePictureBuffers();
    } else {
      LOG(ERROR) << "VDA Error " << result;
      CHECK(false);
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

  // TODO: transition to the error state.
  if (!MakeContextCurrent(stub_))
    CHECK(false) << "CreatePictureBuffer failed to make context current";
  gpu::gles2::ContextGroup* group = stub_->decoder()->GetContextGroup();
  if (!group)
    CHECK(false) << "CreatePictureBuffer failed to get context group";
  gpu::gles2::TextureManager* texture_manager = group->texture_manager();
  if (!texture_manager)
    CHECK(false) << "CreatePictureBuffer failed to get texture manager";

  gfx::Size size = decoder_->GetPicSize();

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

  base::win::ScopedComPtr<ID3D11Texture2D> out_texture;
  HRESULT hr = device_->CreateTexture2D(&texture_desc, nullptr,
                                        out_texture.GetAddressOf());
  CHECK(SUCCEEDED(hr));

  // Drop any old pictures.
  for (auto& buffer : picture_buffers_)
    CHECK(!buffer->in_picture_use());

  // TODO(liberato): Not sure if this is sufficient for cleanup.
  // It's almost sufficient, since any pictures that are in use by the client
  // will keep everything around properly.  The GLImage is held by the Texture,
  // and the Texture is held by the client.  However, nothing cleans up the
  // D3D11 stuff.  Some of it should be cleaned up per-picture.  Other stuff
  // should be cleaned up by the last PictureBuffer, i think.
  // I've also not checked that the DXGI GLImage handles cleanup properly.
  // D3D11PictureBuffer doesn't seem to do anything either.
  picture_buffers_.clear();

  // In order to re-use the 264 accelerator, we temporarily create
  // PictureBuffers and wrap them up as D3D11PictureBuffers.  Once we remove
  // the VDA implementation, we can remove the PictureBuffer and put everything
  // into D3D11PictureBuffer.  However, since the VDA version works right now,
  // more or less, I don't want break it.  I also don't want to fork the
  // implementations, since there will likely be fixes that should be applied
  // to both.
  //
  // We could create VDA and MVD subclasses of D3D11PictureBuffer that hide
  // whether a PictureBuffer is used or not.  D3D11PictureBuffer would provide
  // accessors for all the stuff that the accelerator needs.  Our impl would
  // just store the TextureRef etc. directly.  Seems like too much work for a
  // very temporary problem though.

  // Create each picture buffer.
  // TODO(liberato): We could also do this (and the work in the picture) when
  // outputting a new VideoFrame.  Then, we could just wait for the GLImage to
  // be destructed.  This is how MCVD does it.
  //
  // I think that the stream is connected to the texture directly, so we'd need
  // to make a new stream for each frame.  i have no idea how expensive that
  // is.  For now, we watch for the VideoFrame's mailbox to be deleted, and
  // re-use the textures / stream.
  for (size_t i = 0; i < num_buffers; i++) {
    // Create new Textures.
    std::vector<scoped_refptr<gpu::gles2::TextureRef>> texture_refs;
    PictureBuffer::TextureIds client_ids;
    PictureBuffer::TextureIds service_ids;
    const uint32_t target = GL_TEXTURE_EXTERNAL_OES;
    const int textures_per_picture = 2;  // From the VDA
    gpu::MailboxHolder mailbox_holders[VideoFrame::kMaxPlanes];
    for (int texture_idx = 0; texture_idx < textures_per_picture;
         texture_idx++) {
      texture_refs.push_back(decoder_helper_->CreateTexture(
          target, GL_RGBA, size.width(), size.height(), GL_RGBA,
          GL_UNSIGNED_BYTE));
      // Nothing uses the client id, which is fortunate since it doesn't exist.
      client_ids.push_back(0);
      service_ids.push_back(texture_refs[texture_idx]->service_id());

      // Make a mailbox and holder for each texture.  We'll wrap these with a
      // VideoFrame later.
      gpu::Mailbox mailbox =
          decoder_helper_->CreateMailbox(texture_refs[texture_idx].get());
      mailbox_holders[texture_idx] = gpu::MailboxHolder(
          mailbox, gpu::SyncToken(), GL_TEXTURE_EXTERNAL_OES);
    }

    PictureBuffer buffer(i, size, client_ids, service_ids, target,
                         PIXEL_FORMAT_NV12);

    picture_buffers_.push_back(base::MakeUnique<D3D11PictureBuffer>(
        buffer, i, texture_refs, mailbox_holders));

    picture_buffers_[i]->Init(video_device_, out_texture, decoder_guid_);

    // Bind the image to each texture.
    for (int texture_idx = 0; texture_idx < texture_refs.size();
         texture_idx++) {
      texture_manager->SetLevelImage(texture_refs[texture_idx].get(),
                                     GL_TEXTURE_EXTERNAL_OES, 0,
                                     picture_buffers_[i]->gl_image().get(),
                                     gpu::gles2::Texture::ImageState::BOUND);
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

size_t D3D11VideoDecoderImpl::input_buffer_id() const {
  // NOTE: nobody uses this for anything.  it just gets returned to us with
  // OutputResult.  It can be removed once we replace the VDA.
  return 0;
}

void D3D11VideoDecoderImpl::OutputResult(D3D11PictureBuffer* buffer,
                                         size_t input_buffer_id) {
  buffer->set_in_client_use(true);

  // Note: The pixel format doesn't matter.
  gfx::Rect visible_rect(buffer->picture_buffer().size());
  gfx::Size natural_size = buffer->picture_buffer().size();
  base::TimeDelta timestamp = buffer->timestamp_;
  auto frame = VideoFrame::WrapNativeTextures(
      buffer->picture_buffer().pixel_format(), buffer->mailbox_holders(),
      VideoFrame::ReleaseMailboxCB(), buffer->picture_buffer().size(),
      visible_rect, natural_size, timestamp);

  // TODO(liberato): The VideoFrame release callback will not be called after
  // MojoVideoDecoderService is destructed.  See VideoFrameFactoryImpl.
  // NOTE: i think that we drop the picture buffers, which might work okay.
  output_cb_.Run(base::Bind(&D3D11VideoDecoderImpl::OnMailboxReleased,
                            weak_factory_.GetWeakPtr(), buffer),
                 frame);
}

void D3D11VideoDecoderImpl::OnMailboxReleased(
    D3D11PictureBuffer* buffer,
    const gpu::SyncToken& sync_token) {
  // TODO(liberato): what about the sync token?
  buffer->set_in_client_use(false);

  // Also re-start decoding in case it was waiting for more pictures.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::Bind(&D3D11VideoDecoderImpl::DoDecode, weak_factory_.GetWeakPtr()));
}

base::WeakPtr<D3D11VideoDecoderImpl> D3D11VideoDecoderImpl::GetWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace media
