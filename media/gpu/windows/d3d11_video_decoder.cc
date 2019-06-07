// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/windows/d3d11_video_decoder.h"

#include <d3d11_4.h>
#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted_delete_on_sequence.h"
#include "base/metrics/histogram_macros.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/cdm_context.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_log.h"
#include "media/base/media_switches.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder_config.h"
#include "media/base/video_frame.h"
#include "media/base/video_util.h"
#include "media/gpu/windows/d3d11_picture_buffer.h"
#include "media/gpu/windows/d3d11_video_context_wrapper.h"
#include "media/gpu/windows/d3d11_video_decoder_impl.h"
#include "ui/gl/gl_angle_util_win.h"
#include "ui/gl/gl_switches.h"

namespace media {

namespace {

#define INRANGE(_profile, codecname) \
  (_profile >= codecname##PROFILE_MIN && _profile <= codecname##PROFILE_MAX)

bool IsVP9(const VideoDecoderConfig& config) {
  return INRANGE(config.profile(), VP9);
}

bool IsH264(const VideoDecoderConfig& config) {
  return INRANGE(config.profile(), H264);
}

#undef INRANGE

// Holder class, so that we don't keep creating CommandBufferHelpers every time
// somebody calls a callback.  We can't actually create it until we're on the
// right thread.
struct CommandBufferHelperHolder
    : base::RefCountedDeleteOnSequence<CommandBufferHelperHolder> {
  CommandBufferHelperHolder(
      scoped_refptr<base::SequencedTaskRunner> task_runner)
      : base::RefCountedDeleteOnSequence<CommandBufferHelperHolder>(
            std::move(task_runner)) {}
  scoped_refptr<CommandBufferHelper> helper;

 private:
  ~CommandBufferHelperHolder() = default;
  friend class base::RefCountedDeleteOnSequence<CommandBufferHelperHolder>;
  friend class base::DeleteHelper<CommandBufferHelperHolder>;

  DISALLOW_COPY_AND_ASSIGN(CommandBufferHelperHolder);
};

scoped_refptr<CommandBufferHelper> CreateCommandBufferHelper(
    base::RepeatingCallback<gpu::CommandBufferStub*()> get_stub_cb,
    scoped_refptr<CommandBufferHelperHolder> holder) {
  gpu::CommandBufferStub* stub = get_stub_cb.Run();
  if (!stub)
    return nullptr;

  DCHECK(holder);
  if (!holder->helper)
    holder->helper = CommandBufferHelper::Create(stub);

  return holder->helper;
}

}  // namespace

D3D11_VIDEO_DECODER_DESC TextureSelector::DecoderDescriptor(gfx::Size size) {
  D3D11_VIDEO_DECODER_DESC desc = {};
  desc.Guid = decoder_guid;
  desc.SampleWidth = size.width();
  desc.SampleHeight = size.height();
  desc.OutputFormat = dxgi_format;
  return desc;
}

D3D11_TEXTURE2D_DESC TextureSelector::TextureDescriptor(gfx::Size size) {
  D3D11_TEXTURE2D_DESC texture_desc = {};
  texture_desc.Width = size.width();
  texture_desc.Height = size.height();
  texture_desc.MipLevels = 1;
  texture_desc.ArraySize = TextureSelector::BUFFER_COUNT;
  texture_desc.Format = dxgi_format;
  texture_desc.SampleDesc.Count = 1;
  texture_desc.Usage = D3D11_USAGE_DEFAULT;
  texture_desc.BindFlags = D3D11_BIND_DECODER | D3D11_BIND_SHADER_RESOURCE;

  // Decode swap chains do not support shared resources.
  // TODO(sunnyps): Find a workaround for when the decoder moves to its own
  // thread and D3D device.  See https://crbug.com/911847
  texture_desc.MiscFlags =
      supports_swap_chain_ ? 0 : D3D11_RESOURCE_MISC_SHARED;

  if (is_encrypted_)
    texture_desc.MiscFlags |= D3D11_RESOURCE_MISC_HW_PROTECTED;

  return texture_desc;
}

bool TextureSelector::SupportsDevice(
    Microsoft::WRL::ComPtr<ID3D11VideoDevice> video_device) {
  for (UINT i = video_device->GetVideoDecoderProfileCount(); i--;) {
    GUID profile = {};
    if (SUCCEEDED(video_device->GetVideoDecoderProfile(i, &profile))) {
      if (profile == decoder_guid)
        return true;
    }
  }
  return false;
}

// static
std::unique_ptr<TextureSelector> TextureSelector::Create(
    const VideoDecoderConfig& config) {
  if (config.profile() == VP9PROFILE_PROFILE2) {
    return std::make_unique<TextureSelector>(
        PIXEL_FORMAT_YUV420P10, DXGI_FORMAT_P010,
        D3D11_DECODER_PROFILE_VP9_VLD_10BIT_PROFILE2, config.is_encrypted(),
        false);
  }

  bool supports_nv12_decode_swap_chain = base::FeatureList::IsEnabled(
      features::kDirectCompositionUseNV12DecodeSwapChain);

  if (config.profile() == VP9PROFILE_PROFILE0) {
    return std::make_unique<TextureSelector>(
        PIXEL_FORMAT_NV12, DXGI_FORMAT_NV12,
        D3D11_DECODER_PROFILE_VP9_VLD_PROFILE0, config.is_encrypted(),
        supports_nv12_decode_swap_chain);
  }

  if (IsH264(config)) {
    return std::make_unique<TextureSelector>(
        PIXEL_FORMAT_NV12, DXGI_FORMAT_NV12,
        D3D11_DECODER_PROFILE_H264_VLD_NOFGT, config.is_encrypted(),
        supports_nv12_decode_swap_chain);
  }

  return nullptr;
}

std::unique_ptr<VideoDecoder> D3D11VideoDecoder::Create(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    std::unique_ptr<MediaLog> media_log,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    base::RepeatingCallback<gpu::CommandBufferStub*()> get_stub_cb,
    D3D11VideoDecoder::GetD3D11DeviceCB get_d3d11_device_cb,
    SupportedConfigs supported_configs) {
  // We create |impl_| on the wrong thread, but we never use it here.
  // Note that the output callback will hop to our thread, post the video
  // frame, and along with a callback that will hop back to the impl thread
  // when it's released.
  // Note that we WrapUnique<VideoDecoder> rather than D3D11VideoDecoder to make
  // this castable; the deleters have to match.
  std::unique_ptr<MediaLog> cloned_media_log = media_log->Clone();
  auto get_helper_cb =
      base::BindRepeating(CreateCommandBufferHelper, std::move(get_stub_cb),
                          scoped_refptr<CommandBufferHelperHolder>(
                              new CommandBufferHelperHolder(gpu_task_runner)));
  return base::WrapUnique<VideoDecoder>(
      new D3D11VideoDecoder(std::move(gpu_task_runner), std::move(media_log),
                            gpu_preferences, gpu_workarounds,
                            std::make_unique<D3D11VideoDecoderImpl>(
                                std::move(cloned_media_log), get_helper_cb),
                            get_helper_cb, std::move(get_d3d11_device_cb),
                            std::move(supported_configs)));
}

D3D11VideoDecoder::D3D11VideoDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    std::unique_ptr<MediaLog> media_log,
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    std::unique_ptr<D3D11VideoDecoderImpl> impl,
    base::RepeatingCallback<scoped_refptr<CommandBufferHelper>()> get_helper_cb,
    GetD3D11DeviceCB get_d3d11_device_cb,
    SupportedConfigs supported_configs)
    : media_log_(std::move(media_log)),
      impl_(std::move(impl)),
      impl_task_runner_(std::move(gpu_task_runner)),
      gpu_preferences_(gpu_preferences),
      gpu_workarounds_(gpu_workarounds),
      get_d3d11_device_cb_(std::move(get_d3d11_device_cb)),
      get_helper_cb_(std::move(get_helper_cb)),
      supported_configs_(std::move(supported_configs)),
      weak_factory_(this) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(media_log_);

  impl_weak_ = impl_->GetWeakPtr();
}

D3D11VideoDecoder::~D3D11VideoDecoder() {
  // Post destruction to the main thread.  When this executes, it will also
  // cancel pending callbacks into |impl_| via |impl_weak_|.  Callbacks out
  // from |impl_| will be cancelled by |weak_factory_| when we return.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (impl_task_runner_->RunsTasksInCurrentSequence())
    impl_.reset();
  else
    impl_task_runner_->DeleteSoon(FROM_HERE, std::move(impl_));

  // Explicitly destroy the decoder, since it can reference picture buffers.
  accelerated_video_decoder_.reset();
}

std::string D3D11VideoDecoder::GetDisplayName() const {
  return "D3D11VideoDecoder";
}

HRESULT D3D11VideoDecoder::InitializeAcceleratedDecoder(
    const VideoDecoderConfig& config,
    CdmProxyContext* proxy_context,
    Microsoft::WRL::ComPtr<ID3D11VideoDecoder> video_decoder) {
  // If we got an 11.1 D3D11 Device, we can use a |ID3D11VideoContext1|,
  // otherwise we have to make sure we only use a |ID3D11VideoContext|.
  HRESULT hr;

  // |device_context_| is the primary display context, but currently
  // we share it for decoding purposes.
  auto video_context = VideoContextWrapper::CreateWrapper(usable_feature_level_,
                                                          device_context_, &hr);

  if (!SUCCEEDED(hr))
    return hr;

  if (IsVP9(config)) {
    accelerated_video_decoder_ = std::make_unique<VP9Decoder>(
        std::make_unique<D3D11VP9Accelerator>(
            this, media_log_.get(), proxy_context, video_decoder, video_device_,
            std::move(video_context)),
        config.color_space_info());
    return hr;
  }

  if (IsH264(config)) {
    accelerated_video_decoder_ = std::make_unique<H264Decoder>(
        std::make_unique<D3D11H264Accelerator>(
            this, media_log_.get(), proxy_context, video_decoder, video_device_,
            std::move(video_context)),
        config.color_space_info());
    return hr;
  }

  return E_FAIL;
}

void D3D11VideoDecoder::Initialize(const VideoDecoderConfig& config,
                                   bool low_delay,
                                   CdmContext* cdm_context,
                                   InitCB init_cb,
                                   const OutputCB& output_cb,
                                   const WaitingCB& waiting_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(output_cb);
  DCHECK(waiting_cb);

  state_ = State::kInitializing;

  config_ = config;
  init_cb_ = std::move(init_cb);
  output_cb_ = output_cb;
  waiting_cb_ = waiting_cb;

  // Verify that |config| matches one of the supported configurations.  This
  // helps us skip configs that are supported by the VDA but not us, since
  // GpuMojoMediaClient merges them.  This is not hacky, even in the tiniest
  // little bit, nope.  Definitely not.  Convinced?
  bool is_supported = false;
  for (const auto& supported_config : supported_configs_) {
    if (supported_config.Matches(config)) {
      is_supported = true;
      break;
    }
  }

  if (!is_supported) {
    NotifyError("D3D11VideoDecoder does not support this config");
    return;
  }

  // Initialize the video decoder.

  // Note that we assume that this is the ANGLE device, since we don't implement
  // texture sharing properly.  That also implies that this is the GPU main
  // thread, since we use non-threadsafe properties of the device (e.g., we get
  // the immediate context).
  //
  // Also note that we don't technically have a guarantee that the ANGLE device
  // will use the most recent version of D3D11; it might be configured to use
  // D3D9.  In practice, though, it seems to use 11.1 if it's available, unless
  // it's been specifically configured via switch to avoid d3d11.
  //
  // TODO(liberato): On re-init, we can probably re-use the device.
  device_ = get_d3d11_device_cb_.Run();
  usable_feature_level_ = device_->GetFeatureLevel();

  if (!device_) {
    // This happens if, for example, if chrome is configured to use
    // D3D9 for ANGLE.
    NotifyError("ANGLE did not provide D3D11 device");
    return;
  }
  device_->GetImmediateContext(device_context_.ReleaseAndGetAddressOf());

  HRESULT hr;

  // TODO(liberato): Handle cleanup better.  Also consider being less chatty in
  // the logs, since this will fall back.

  hr = device_.CopyTo(video_device_.ReleaseAndGetAddressOf());
  if (!SUCCEEDED(hr)) {
    NotifyError("Failed to get video device");
    return;
  }

  texture_selector_ = TextureSelector::Create(config);
  if (!texture_selector_) {
    NotifyError("D3DD11: Config provided unsupported profile");
    return;
  }

  if (!texture_selector_->SupportsDevice(video_device_)) {
    NotifyError("D3D11: Device does not support decoder GUID");
    return;
  }

  // TODO(liberato): dxva does this.  don't know if we need to.
  Microsoft::WRL::ComPtr<ID3D11Multithread> multi_threaded;
  hr = device_->QueryInterface(IID_PPV_ARGS(&multi_threaded));
  if (!SUCCEEDED(hr)) {
    NotifyError("Failed to query ID3D11Multithread");
    return;
  }
  // TODO(liberato): This is a hack, since the unittest returns
  // success without providing |multi_threaded|.
  if (multi_threaded)
    multi_threaded->SetMultithreadProtected(TRUE);

  D3D11_VIDEO_DECODER_DESC desc =
      texture_selector_->DecoderDescriptor(config.coded_size());
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

    if (config.is_encrypted() && dec_config.guidConfigBitstreamEncryption !=
                                     D3D11_DECODER_ENCRYPTION_HW_CENC) {
      // For encrypted media, it has to use HW CENC decoder config.
      continue;
    }

    if (IsVP9(config) && dec_config.ConfigBitstreamRaw == 1) {
      // DXVA VP9 specification mentions ConfigBitstreamRaw "shall be 1".
      found = true;
      break;
    }

    if (IsH264(config) && dec_config.ConfigBitstreamRaw == 2) {
      // ConfigBitstreamRaw == 2 means the decoder uses DXVA_Slice_H264_Short.
      found = true;
      break;
    }
  }
  if (!found) {
    NotifyError("Failed to find decoder config");
    return;
  }

  Microsoft::WRL::ComPtr<ID3D11VideoDecoder> video_decoder;
  hr = video_device_->CreateVideoDecoder(
      &desc, &dec_config, video_decoder.ReleaseAndGetAddressOf());
  if (!video_decoder.Get()) {
    NotifyError("Failed to create a video decoder");
    return;
  }

  CdmProxyContext* proxy_context = nullptr;
#if BUILDFLAG(ENABLE_LIBRARY_CDMS)
  if (cdm_context)
    proxy_context = cdm_context->GetCdmProxyContext();
#endif

  // Ensure that if we are encrypted, that we have a CDM.
  if (config_.is_encrypted() && !proxy_context) {
    NotifyError("Video stream is encrypted, but no cdm was found");
    return;
  }

  hr = InitializeAcceleratedDecoder(config, proxy_context, video_decoder);

  if (!SUCCEEDED(hr)) {
    NotifyError("Failed to get device context");
    return;
  }

  // At this point, playback is supported so add a line in the media log to help
  // us figure that out.
  media_log_->AddEvent(
      media_log_->CreateStringEvent(MediaLogEvent::MEDIA_INFO_LOG_ENTRY, "info",
                                    "Video is supported by D3D11VideoDecoder"));

  // |cdm_context| could be null for clear playback.
  // TODO(liberato): On re-init, should this still happen?
  if (cdm_context) {
    new_key_callback_registration_ =
        cdm_context->RegisterEventCB(base::BindRepeating(
            &D3D11VideoDecoder::OnCdmContextEvent, weak_factory_.GetWeakPtr()));
  }

  auto impl_init_cb = base::BindOnce(&D3D11VideoDecoder::OnGpuInitComplete,
                                     weak_factory_.GetWeakPtr());

  auto get_picture_buffer_cb =
      base::BindRepeating(&D3D11VideoDecoder::ReceivePictureBufferFromClient,
                          weak_factory_.GetWeakPtr());

  // Initialize the gpu side.  We wait until everything else is initialized,
  // since we allow it to call us back re-entrantly to reduce latency.  Note
  // that if we're not on the same thread, then we should probably post the
  // call earlier, since re-entrancy won't be an issue.
  if (impl_task_runner_->RunsTasksInCurrentSequence()) {
    impl_->Initialize(std::move(impl_init_cb),
                      std::move(get_picture_buffer_cb));
    return;
  }

  // Bind our own init / output cb that hop to this thread, so we don't call
  // the originals on some other thread.
  // Important but subtle note: base::Bind will copy |config_| since it's a
  // const ref.
  impl_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&D3D11VideoDecoderImpl::Initialize, impl_weak_,
                     BindToCurrentLoop(std::move(impl_init_cb)),
                     BindToCurrentLoop(std::move(get_picture_buffer_cb))));
}

void D3D11VideoDecoder::ReceivePictureBufferFromClient(
    scoped_refptr<D3D11PictureBuffer> buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  // We may decode into this buffer again.
  // Note that |buffer| might no longer be in |picture_buffers_| if we've
  // replaced them.  That's okay.
  buffer->set_in_client_use(false);

  // Also re-start decoding in case it was waiting for more pictures.
  DoDecode();
}

void D3D11VideoDecoder::OnGpuInitComplete(bool success) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (!init_cb_) {
    // We already failed, so just do nothing.
    DCHECK_EQ(state_, State::kError);
    return;
  }

  DCHECK_EQ(state_, State::kInitializing);

  if (!success) {
    NotifyError("Gpu init failed");
    return;
  }

  state_ = State::kRunning;
  std::move(init_cb_).Run(true);
}

void D3D11VideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                               DecodeCB decode_cb) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ == State::kError) {
    // TODO(liberato): consider posting, though it likely doesn't matter.
    std::move(decode_cb).Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  input_buffer_queue_.push_back(
      std::make_pair(std::move(buffer), std::move(decode_cb)));

  // Post, since we're not supposed to call back before this returns.  It
  // probably doesn't matter since we're in the gpu process anyway.
  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&D3D11VideoDecoder::DoDecode, weak_factory_.GetWeakPtr()));
}

void D3D11VideoDecoder::DoDecode() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  if (state_ != State::kRunning) {
    DVLOG(2) << __func__ << ": Do nothing in " << static_cast<int>(state_)
             << " state.";
    return;
  }

  if (!current_buffer_) {
    if (input_buffer_queue_.empty()) {
      return;
    }
    current_buffer_ = std::move(input_buffer_queue_.front().first);
    current_decode_cb_ = std::move(input_buffer_queue_.front().second);
    input_buffer_queue_.pop_front();
    if (current_buffer_->end_of_stream()) {
      // Flush, then signal the decode cb once all pictures have been output.
      current_buffer_ = nullptr;
      if (!accelerated_video_decoder_->Flush()) {
        // This will also signal error |current_decode_cb_|.
        NotifyError("Flush failed");
        return;
      }
      // Pictures out output synchronously during Flush.  Signal the decode
      // cb now.
      std::move(current_decode_cb_).Run(DecodeStatus::OK);
      return;
    }
    // This must be after checking for EOS because there is no timestamp for an
    // EOS buffer.
    current_timestamp_ = current_buffer_->timestamp();

    accelerated_video_decoder_->SetStream(-1, current_buffer_->data(),
                                          current_buffer_->data_size(),
                                          current_buffer_->decrypt_config());
  }

  while (true) {
    // If we transition to the error state, then stop here.
    if (state_ == State::kError)
      return;

    media::AcceleratedVideoDecoder::DecodeResult result =
        accelerated_video_decoder_->Decode();
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
    } else if (result == media::AcceleratedVideoDecoder::kTryAgain) {
      state_ = State::kWaitingForNewKey;
      waiting_cb_.Run(WaitingReason::kNoDecryptionKey);
      // Another DoDecode() task would be posted in OnCdmContextEvent().
      return;
    } else {
      LOG(ERROR) << "VDA Error " << result;
      NotifyError("Accelerated decode failed");
      return;
    }
  }

  base::ThreadTaskRunnerHandle::Get()->PostTask(
      FROM_HERE,
      base::BindOnce(&D3D11VideoDecoder::DoDecode, weak_factory_.GetWeakPtr()));
}

void D3D11VideoDecoder::Reset(base::OnceClosure closure) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_NE(state_, State::kInitializing);

  current_buffer_ = nullptr;
  if (current_decode_cb_)
    std::move(current_decode_cb_).Run(DecodeStatus::ABORTED);

  for (auto& queue_pair : input_buffer_queue_)
    std::move(queue_pair.second).Run(DecodeStatus::ABORTED);
  input_buffer_queue_.clear();

  // TODO(liberato): how do we signal an error?
  accelerated_video_decoder_->Reset();

  if (state_ == State::kWaitingForReset && config_.is_encrypted()) {
    // On a hardware context loss event, a new swap chain has to be created (in
    // the compositor). By clearing the picture buffers, next DoDecode() call
    // will create a new texture. This makes the compositor to create a new swap
    // chain.
    // More detailed explanation at crbug.com/858286
    picture_buffers_.clear();
  }

  // Transition out of kWaitingForNewKey since the new buffer could be clear or
  // have a different key ID. Transition out of kWaitingForReset since reset
  // just happened.
  if (state_ == State::kWaitingForNewKey || state_ == State::kWaitingForReset)
    state_ = State::kRunning;

  std::move(closure).Run();
}

bool D3D11VideoDecoder::NeedsBitstreamConversion() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return true;
}

bool D3D11VideoDecoder::CanReadWithoutStalling() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return false;
}

int D3D11VideoDecoder::GetMaxDecodeRequests() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  return 4;
}

void D3D11VideoDecoder::CreatePictureBuffers() {
  // TODO(liberato): When we run off the gpu main thread, this call will need
  // to signal success / failure asynchronously.  We'll need to transition into
  // a "waiting for pictures" state, since D3D11PictureBuffer will post the gpu
  // thread work.
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(texture_selector_);
  gfx::Size size = accelerated_video_decoder_->GetPicSize();

  D3D11_TEXTURE2D_DESC texture_desc =
      texture_selector_->TextureDescriptor(size);

  Microsoft::WRL::ComPtr<ID3D11Texture2D> out_texture;
  HRESULT hr = device_->CreateTexture2D(&texture_desc, nullptr,
                                        out_texture.ReleaseAndGetAddressOf());
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
  for (size_t i = 0; i < TextureSelector::BUFFER_COUNT; i++) {
    auto processor = std::make_unique<DefaultTexture2DWrapper>(out_texture);
    picture_buffers_.push_back(new D3D11PictureBuffer(
        GL_TEXTURE_EXTERNAL_OES, std::move(processor), size, i));
    if (!picture_buffers_[i]->Init(get_helper_cb_, video_device_,
                                   texture_selector_->decoder_guid,
                                   textures_per_picture, media_log_->Clone())) {
      NotifyError("Unable to allocate PictureBuffer");
      return;
    }
  }
}

D3D11PictureBuffer* D3D11VideoDecoder::GetPicture() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  for (auto& buffer : picture_buffers_) {
    if (!buffer->in_client_use() && !buffer->in_picture_use()) {
      buffer->timestamp_ = current_timestamp_;
      return buffer.get();
    }
  }

  return nullptr;
}

void D3D11VideoDecoder::OutputResult(const CodecPicture* picture,
                                     D3D11PictureBuffer* picture_buffer) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(texture_selector_);

  picture_buffer->set_in_client_use(true);

  // Note: The pixel format doesn't matter.
  gfx::Rect visible_rect = picture->visible_rect();
  if (visible_rect.IsEmpty())
    visible_rect = config_.visible_rect();

  // TODO(https://crbug.com/843150): Use aspect ratio from decoder (SPS) if
  // stream metadata doesn't overrride it.
  double pixel_aspect_ratio = config_.GetPixelAspectRatio();

  base::TimeDelta timestamp = picture_buffer->timestamp_;

  MailboxHolderArray mailbox_holders;
  if (!picture_buffer->ProcessTexture(&mailbox_holders)) {
    NotifyError("Unable to process texture");
    return;
  }

  scoped_refptr<VideoFrame> frame = VideoFrame::WrapNativeTextures(
      texture_selector_->pixel_format, mailbox_holders,
      VideoFrame::ReleaseMailboxCB(), picture_buffer->size(), visible_rect,
      GetNaturalSize(visible_rect, pixel_aspect_ratio), timestamp);

  // TODO(liberato): bind this to the gpu main thread.
  frame->SetReleaseMailboxCB(media::BindToCurrentLoop(
      base::BindOnce(&D3D11VideoDecoderImpl::OnMailboxReleased, impl_weak_,
                     scoped_refptr<D3D11PictureBuffer>(picture_buffer))));
  frame->metadata()->SetBoolean(VideoFrameMetadata::POWER_EFFICIENT, true);
  // For NV12, overlay is allowed by default. If the decoder is going to support
  // non-NV12 textures, then this may have to be conditionally set. Also note
  // that ALLOW_OVERLAY is required for encrypted video path.
  frame->metadata()->SetBoolean(VideoFrameMetadata::ALLOW_OVERLAY, true);

  if (config_.is_encrypted()) {
    frame->metadata()->SetBoolean(VideoFrameMetadata::PROTECTED_VIDEO, true);
    frame->metadata()->SetBoolean(VideoFrameMetadata::HW_PROTECTED, true);
  }

  frame->set_color_space(picture->get_colorspace().ToGfxColorSpace());
  output_cb_.Run(frame);
}

void D3D11VideoDecoder::OnCdmContextEvent(CdmContext::Event event) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DVLOG(1) << __func__ << ": event = " << static_cast<int>(event);

  if (state_ == State::kInitializing || state_ == State::kError) {
    DVLOG(1) << "Do nothing in " << static_cast<int>(state_) << " state.";
    return;
  }

  switch (event) {
    case CdmContext::Event::kHasAdditionalUsableKey:
      // Note that this event may happen before DoDecode() because the key
      // acquisition stack runs independently of the media decoding stack.
      // So if this isn't in kWaitingForNewKey state no "resuming" is
      // required therefore no special action taken here.
      if (state_ != State::kWaitingForNewKey)
        return;

      state_ = State::kRunning;
      base::ThreadTaskRunnerHandle::Get()->PostTask(
          FROM_HERE, base::BindOnce(&D3D11VideoDecoder::DoDecode,
                                    weak_factory_.GetWeakPtr()));
      return;

    case CdmContext::Event::kHardwareContextLost:
      state_ = State::kWaitingForReset;
      waiting_cb_.Run(WaitingReason::kDecoderStateLost);
      return;
  }
}

void D3D11VideoDecoder::NotifyError(const char* reason) {
  state_ = State::kError;
  DLOG(ERROR) << reason;
  media_log_->AddEvent(media_log_->CreateStringEvent(
      MediaLogEvent::MEDIA_ERROR_LOG_ENTRY, "error", reason));

  if (init_cb_)
    std::move(init_cb_).Run(false);

  if (current_decode_cb_)
    std::move(current_decode_cb_).Run(DecodeStatus::DECODE_ERROR);

  for (auto& queue_pair : input_buffer_queue_)
    std::move(queue_pair.second).Run(DecodeStatus::DECODE_ERROR);
  input_buffer_queue_.clear();
}

// static
std::vector<SupportedVideoDecoderConfig>
D3D11VideoDecoder::GetSupportedVideoDecoderConfigs(
    const gpu::GpuPreferences& gpu_preferences,
    const gpu::GpuDriverBugWorkarounds& gpu_workarounds,
    GetD3D11DeviceCB get_d3d11_device_cb) {
  const std::string uma_name("Media.D3D11.WasVideoSupported");

  // Must allow zero-copy of nv12 textures.
  if (!gpu_preferences.enable_zero_copy_dxgi_video) {
    UMA_HISTOGRAM_ENUMERATION(uma_name,
                              NotSupportedReason::kZeroCopyNv12Required);
    return {};
  }

  // This workaround accounts for almost half of all startup results, and it's
  // unclear that it's relevant here.
  if (!base::FeatureList::IsEnabled(kD3D11VideoDecoderIgnoreWorkarounds)) {
    if (gpu_workarounds.disable_dxgi_zero_copy_video) {
      UMA_HISTOGRAM_ENUMERATION(uma_name,
                                NotSupportedReason::kZeroCopyVideoRequired);
      return {};
    }
  }

  // Remember that this might query the angle device, so this won't work if
  // we're not on the GPU main thread.  Also remember that devices are thread
  // safe (contexts are not), so we could use the angle device from any thread
  // as long as we're not calling into possible not-thread-safe things to get
  // it.  I.e., if this cached it, then it'd be fine.  It's up to our caller
  // to guarantee that, though.
  //
  // Note also that, currently, we are called from the GPU main thread only.
  auto d3d11_device = get_d3d11_device_cb.Run();
  if (!d3d11_device) {
    UMA_HISTOGRAM_ENUMERATION(uma_name,
                              NotSupportedReason::kCouldNotGetD3D11Device);
    return {};
  }

  D3D_FEATURE_LEVEL usable_feature_level = d3d11_device->GetFeatureLevel();

  const bool allow_encrypted =
      (usable_feature_level > D3D_FEATURE_LEVEL_11_0) &&
      base::FeatureList::IsEnabled(kHardwareSecureDecryption);

  std::vector<SupportedVideoDecoderConfig> configs;

  // Now check specific configs.
  // For now, just return something that matches everything, since that's
  // effectively what the workaround in mojo_video_decoder does.  Eventually, we
  // should check resolutions and guids from the device we just created for both
  // portrait and landscape orientations.
  const gfx::Size min_resolution(64, 64);
  const gfx::Size max_resolution(8192, 8192);  // Profile or landscape 8k

  // Push H264 configs, except HIGH10.
  configs.push_back(SupportedVideoDecoderConfig(
      H264PROFILE_MIN,  // profile_min
      static_cast<VideoCodecProfile>(H264PROFILE_HIGH10PROFILE -
                                     1),  // profile_max
      min_resolution,                     // coded_size_min
      max_resolution,                     // coded_size_max
      allow_encrypted,                    // allow_encrypted
      false));                            // require_encrypted
  configs.push_back(SupportedVideoDecoderConfig(
      static_cast<VideoCodecProfile>(H264PROFILE_HIGH10PROFILE +
                                     1),  // profile_min
      H264PROFILE_MAX,                    // profile_max
      min_resolution,                     // coded_size_min
      max_resolution,                     // coded_size_max
      allow_encrypted,                    // allow_encrypted
      false));                            // require_encrypted

  configs.push_back(
      SupportedVideoDecoderConfig(VP9PROFILE_PROFILE0,  // profile_min
                                  VP9PROFILE_PROFILE0,  // profile_max
                                  min_resolution,       // coded_size_min
                                  max_resolution,       // coded_size_max
                                  allow_encrypted,      // allow_encrypted
                                  false));              // require_encrypted

  // TODO(liberato): Should we separate out h264, vp9, and encrypted?
  UMA_HISTOGRAM_ENUMERATION(uma_name, NotSupportedReason::kVideoIsSupported);

  return configs;
}

}  // namespace media
