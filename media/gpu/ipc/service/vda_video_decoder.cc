// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/gpu/ipc/service/vda_video_decoder.h"

#include <string.h>

#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "media/base/bitstream_buffer.h"
#include "media/base/decoder_buffer.h"
#include "media/base/video_codecs.h"
#include "media/base/video_types.h"
#include "media/gpu/fake_video_decode_accelerator.h"
#include "media/video/picture.h"
#include "ui/gfx/geometry/rect.h"

namespace media {

namespace {

// Generates nonnegative bitstream buffer IDs, which are assumed to be unique.
int32_t NextID(int32_t* counter) {
  int32_t value = *counter;
  *counter = (*counter + 1) & 0x3FFFFFFF;
  return value;
}

scoped_refptr<CommandBufferHelper> CreateCommandBufferHelper(
    VdaVideoDecoder::GetStubCB get_stub_cb) {
  gpu::CommandBufferStub* stub = std::move(get_stub_cb).Run();

  if (!stub) {
    DVLOG(1) << "Failed to obtain command buffer stub";
    return nullptr;
  }

  return CommandBufferHelper::Create(stub);
}

std::unique_ptr<VideoDecodeAccelerator> CreateVda(
    scoped_refptr<CommandBufferHelper> command_buffer_helper) {
  std::unique_ptr<VideoDecodeAccelerator> vda(new FakeVideoDecodeAccelerator(
      gfx::Size(320, 240),
      base::BindRepeating(&CommandBufferHelper::MakeContextCurrent,
                          command_buffer_helper)));
  return vda;
}

VideoDecodeAccelerator::SupportedProfiles GetSupportedProfiles() {
  VideoDecodeAccelerator::SupportedProfiles profiles;
  {
    VideoDecodeAccelerator::SupportedProfile profile;
    profile.profile = H264PROFILE_BASELINE;
    profile.max_resolution = gfx::Size(1920, 1088);
    profile.min_resolution = gfx::Size(16, 16);
    profile.encrypted_only = false;
    profiles.push_back(std::move(profile));
  }
  return profiles;
}

VideoDecodeAccelerator::Capabilities GetCapabilities() {
  VideoDecodeAccelerator::Capabilities capabilities;
  capabilities.supported_profiles = GetSupportedProfiles();
  capabilities.flags = 0;
  return capabilities;
}

bool IsProfileSupported(
    const VideoDecodeAccelerator::SupportedProfiles& supported_profiles,
    VideoCodecProfile profile,
    gfx::Size coded_size) {
  for (const auto& supported_profile : supported_profiles) {
    if (supported_profile.profile == profile &&
        !supported_profile.encrypted_only &&
        gfx::Rect(supported_profile.max_resolution)
            .Contains(gfx::Rect(coded_size)) &&
        gfx::Rect(coded_size)
            .Contains(gfx::Rect(supported_profile.min_resolution))) {
      return true;
    }
  }
  return false;
}

}  // namespace

// static
std::unique_ptr<VdaVideoDecoder, std::default_delete<VideoDecoder>>
VdaVideoDecoder::Create(
    scoped_refptr<base::SingleThreadTaskRunner> parent_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    GetStubCB get_stub_cb) {
  // Constructed in a variable to avoid _CheckUniquePtr() PRESUBMIT.py regular
  // expressions, which do not understand custom deleters.
  // TODO(sandersd): Extend base::WrapUnique() to handle this.
  std::unique_ptr<VdaVideoDecoder, std::default_delete<VideoDecoder>> ptr(
      new VdaVideoDecoder(
          std::move(parent_task_runner), std::move(gpu_task_runner),
          base::BindOnce(&PictureBufferManager::Create),
          base::BindOnce(&CreateCommandBufferHelper, std::move(get_stub_cb)),
          base::BindOnce(&CreateVda), base::BindRepeating(&GetCapabilities)));
  return ptr;
}

// TODO(sandersd): Take and use a MediaLog. This will require making
// MojoMediaLog threadsafe.
VdaVideoDecoder::VdaVideoDecoder(
    scoped_refptr<base::SingleThreadTaskRunner> parent_task_runner,
    scoped_refptr<base::SingleThreadTaskRunner> gpu_task_runner,
    CreatePictureBufferManagerCB create_picture_buffer_manager_cb,
    CreateCommandBufferHelperCB create_command_buffer_helper_cb,
    CreateVdaCB create_vda_cb,
    GetVdaCapabilitiesCB get_vda_capabilities_cb)
    : parent_task_runner_(std::move(parent_task_runner)),
      gpu_task_runner_(std::move(gpu_task_runner)),
      create_command_buffer_helper_cb_(
          std::move(create_command_buffer_helper_cb)),
      create_vda_cb_(std::move(create_vda_cb)),
      get_vda_capabilities_cb_(std::move(get_vda_capabilities_cb)),
      timestamps_(128),
      gpu_weak_this_factory_(this),
      parent_weak_this_factory_(this) {
  DVLOG(1) << __func__;
  DCHECK(parent_task_runner_->BelongsToCurrentThread());

  gpu_weak_this_ = gpu_weak_this_factory_.GetWeakPtr();
  parent_weak_this_ = parent_weak_this_factory_.GetWeakPtr();

  picture_buffer_manager_ =
      std::move(create_picture_buffer_manager_cb)
          .Run(base::BindRepeating(&VdaVideoDecoder::ReusePictureBuffer,
                                   gpu_weak_this_));
}

void VdaVideoDecoder::Destroy() {
  DVLOG(1) << __func__;
  DCHECK(parent_task_runner_->BelongsToCurrentThread());

  // TODO(sandersd): The documentation says that Destroy() fires any pending
  // callbacks.

  // Prevent any more callbacks to this thread.
  parent_weak_this_factory_.InvalidateWeakPtrs();

  // Pass ownership of the destruction process over to the GPU thread.
  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VdaVideoDecoder::DestroyOnGpuThread, gpu_weak_this_));
}

void VdaVideoDecoder::DestroyOnGpuThread() {
  DVLOG(2) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  // VDA destruction is likely to result in reentrant calls to
  // NotifyEndOfBitstreamBuffer(). Invalidating |gpu_weak_vda_| ensures that we
  // don't call back into |vda_| during its destruction.
  gpu_weak_vda_factory_ = nullptr;
  vda_ = nullptr;

  delete this;
}

VdaVideoDecoder::~VdaVideoDecoder() {
  DVLOG(1) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK(!gpu_weak_vda_);
}

std::string VdaVideoDecoder::GetDisplayName() const {
  DVLOG(3) << __func__;
  DCHECK(parent_task_runner_->BelongsToCurrentThread());

  return "VdaVideoDecoder";
}

void VdaVideoDecoder::Initialize(
    const VideoDecoderConfig& config,
    bool low_delay,
    CdmContext* cdm_context,
    const InitCB& init_cb,
    const OutputCB& output_cb,
    const WaitingForDecryptionKeyCB& waiting_for_decryption_key_cb) {
  DVLOG(1) << __func__ << "(" << config.AsHumanReadableString() << ")";
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  DCHECK(config.IsValidConfig());
  DCHECK(init_cb_.is_null());
  DCHECK(flush_cb_.is_null());
  DCHECK(reset_cb_.is_null());
  DCHECK(decode_cbs_.empty());

  if (has_error_) {
    parent_task_runner_->PostTask(FROM_HERE, base::BindOnce(init_cb, false));
    return;
  }

  bool reinitializing = config_.IsValidConfig();

  // Store |init_cb| ASAP so that EnterErrorState() can use it.
  init_cb_ = init_cb;
  output_cb_ = output_cb;

  // Verify that the configuration is supported.
  VideoDecodeAccelerator::Capabilities capabilities =
      get_vda_capabilities_cb_.Run();
  DCHECK_EQ(capabilities.flags, 0U);

  if (reinitializing && config.codec() != config_.codec()) {
    DLOG(ERROR) << "Codec cannot be changed";
    EnterErrorState();
    return;
  }

  // TODO(sandersd): Change this to a capability if any VDA starts supporting
  // alpha channels. This is believed to be impossible right now because VPx
  // alpha channel data is passed in side data, which isn't sent to VDAs.
  if (!IsOpaque(config.format())) {
    DVLOG(1) << "Alpha formats are not supported";
    EnterErrorState();
    return;
  }

  if (config.is_encrypted()) {
    DVLOG(1) << "Encrypted streams are not supported";
    EnterErrorState();
    return;
  }

  if (!IsProfileSupported(capabilities.supported_profiles, config.profile(),
                          config.coded_size())) {
    DVLOG(1) << "Unsupported profile";
    EnterErrorState();
    return;
  }

  // The configuration is supported; finish initializing.
  config_ = config;

  if (reinitializing) {
    gpu_task_runner_->PostTask(FROM_HERE,
                               base::BindOnce(&VdaVideoDecoder::InitializeDone,
                                              parent_weak_this_, true));
    return;
  }

  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VdaVideoDecoder::InitializeOnGpuThread, gpu_weak_this_));
}

void VdaVideoDecoder::InitializeOnGpuThread() {
  DVLOG(2) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK(!vda_);

  // Set up |command_buffer_helper_|.
  command_buffer_helper_ = std::move(create_command_buffer_helper_cb_).Run();
  if (!command_buffer_helper_) {
    parent_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(&VdaVideoDecoder::InitializeDone,
                                  parent_weak_this_, false));
    return;
  }
  picture_buffer_manager_->Initialize(gpu_task_runner_, command_buffer_helper_);

  // Create the VDA.
  vda_ = std::move(create_vda_cb_).Run(command_buffer_helper_);
  gpu_weak_vda_factory_.reset(
      new base::WeakPtrFactory<VideoDecodeAccelerator>(vda_.get()));
  gpu_weak_vda_ = gpu_weak_vda_factory_->GetWeakPtr();

  // Convert the configuration and initialize the VDA with it.
  VideoDecodeAccelerator::Config vda_config;
  vda_config.profile = config_.profile();
  // vda_config.cdm_id = [Encrypted streams are not supported]
  // vda_config.overlay_info = [Only used by AVDA]
  vda_config.encryption_scheme = config_.encryption_scheme();
  vda_config.is_deferred_initialization_allowed = false;
  vda_config.initial_expected_coded_size = config_.coded_size();
  vda_config.container_color_space = config_.color_space_info();
  // TODO(sandersd): Plumb |target_color_space| from DefaultRenderFactory.
  // vda_config.target_color_space = [...];
  vda_config.hdr_metadata = config_.hdr_metadata();
  // vda_config.sps = [Only used by AVDA]
  // vda_config.pps = [Only used by AVDA]
  // vda_config.output_mode = [Only used by ARC]
  // vda_config.supported_output_formats = [Only used by PPAPI]

  // TODO(sandersd): TryToSetupDecodeOnSeparateThread().
  bool status = vda_->Initialize(vda_config, this);
  parent_task_runner_->PostTask(FROM_HERE,
                                base::BindOnce(&VdaVideoDecoder::InitializeDone,
                                               parent_weak_this_, status));
}

void VdaVideoDecoder::InitializeDone(bool status) {
  DVLOG(1) << __func__ << "(" << status << ")";
  DCHECK(parent_task_runner_->BelongsToCurrentThread());

  if (has_error_)
    return;

  if (!status) {
    // TODO(sandersd): This adds an unnecessary PostTask().
    EnterErrorState();
    return;
  }

  base::ResetAndReturn(&init_cb_).Run(true);
}

void VdaVideoDecoder::Decode(scoped_refptr<DecoderBuffer> buffer,
                             const DecodeCB& decode_cb) {
  DVLOG(3) << __func__ << "(" << (buffer->end_of_stream() ? "EOS" : "") << ")";
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  DCHECK(init_cb_.is_null());
  DCHECK(flush_cb_.is_null());
  DCHECK(reset_cb_.is_null());
  DCHECK(buffer->end_of_stream() || !buffer->decrypt_config());

  if (has_error_) {
    parent_task_runner_->PostTask(
        FROM_HERE, base::BindOnce(decode_cb, DecodeStatus::DECODE_ERROR));
    return;
  }

  // Convert EOS frame to Flush().
  if (buffer->end_of_stream()) {
    flush_cb_ = decode_cb;
    gpu_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VideoDecodeAccelerator::Flush, gpu_weak_vda_));
    return;
  }

  // Assign a bitstream buffer ID and record the decode request.
  int32_t bitstream_buffer_id = NextID(&bitstream_buffer_id_);
  timestamps_.Put(bitstream_buffer_id, buffer->timestamp());
  decode_cbs_[bitstream_buffer_id] = decode_cb;

  // Copy data into shared memory.
  // TODO(sandersd): Either use a pool of SHM, or adapt the VDAs to be able to
  // use regular memory instead.
  size_t size = buffer->data_size();
  base::SharedMemory mem;
  if (!mem.CreateAndMapAnonymous(size)) {
    DLOG(ERROR) << "Failed to map SHM with size " << size;
    EnterErrorState();
    return;
  }
  memcpy(mem.memory(), buffer->data(), size);

  // Note: Once we take the handle, we must close it ourselves. Since Destroy()
  // has not already been called, we can be sure that |gpu_weak_this_| will be
  // valid.
  BitstreamBuffer bitstream_buffer(bitstream_buffer_id, mem.TakeHandle(), size,
                                   0, buffer->timestamp());
  gpu_task_runner_->PostTask(FROM_HERE,
                             base::BindOnce(&VdaVideoDecoder::DecodeOnGpuThread,
                                            gpu_weak_this_, bitstream_buffer));
}

void VdaVideoDecoder::DecodeOnGpuThread(BitstreamBuffer bitstream_buffer) {
  DVLOG(3) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  if (!gpu_weak_vda_) {
    base::SharedMemory::CloseHandle(bitstream_buffer.handle());
    return;
  }

  vda_->Decode(bitstream_buffer);
}

void VdaVideoDecoder::Reset(const base::RepeatingClosure& reset_cb) {
  DVLOG(2) << __func__;
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  DCHECK(init_cb_.is_null());
  // Note: |flush_cb_| may not be null.
  DCHECK(reset_cb_.is_null());

  if (has_error_) {
    parent_task_runner_->PostTask(FROM_HERE, reset_cb);
    return;
  }

  reset_cb_ = reset_cb;
  gpu_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VideoDecodeAccelerator::Reset, gpu_weak_vda_));
}

bool VdaVideoDecoder::NeedsBitstreamConversion() const {
  DVLOG(3) << __func__;
  DCHECK(parent_task_runner_->BelongsToCurrentThread());

  // TODO(sandersd): Can we move bitstream conversion into VdaVideoDecoder and
  // always return false?
  return config_.codec() == kCodecH264 || config_.codec() == kCodecHEVC;
}

bool VdaVideoDecoder::CanReadWithoutStalling() const {
  DVLOG(3) << __func__;
  DCHECK(parent_task_runner_->BelongsToCurrentThread());

  return picture_buffer_manager_->CanReadWithoutStalling();
}

int VdaVideoDecoder::GetMaxDecodeRequests() const {
  DVLOG(3) << __func__;
  DCHECK(parent_task_runner_->BelongsToCurrentThread());

  return 4;
}

void VdaVideoDecoder::NotifyInitializationComplete(bool success) {
  DVLOG(2) << __func__ << "(" << success << ")";
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  NOTIMPLEMENTED();
}

void VdaVideoDecoder::ProvidePictureBuffers(uint32_t requested_num_of_buffers,
                                            VideoPixelFormat format,
                                            uint32_t textures_per_buffer,
                                            const gfx::Size& dimensions,
                                            uint32_t texture_target) {
  DVLOG(2) << __func__ << "(" << requested_num_of_buffers << ", " << format
           << ", " << textures_per_buffer << ", " << dimensions.ToString()
           << ", " << texture_target << ")";
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  gpu_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VdaVideoDecoder::ProvidePictureBuffersAsync,
                     gpu_weak_this_, requested_num_of_buffers, format,
                     textures_per_buffer, dimensions, texture_target));
}

void VdaVideoDecoder::ProvidePictureBuffersAsync(uint32_t count,
                                                 VideoPixelFormat pixel_format,
                                                 uint32_t planes,
                                                 gfx::Size texture_size,
                                                 GLenum texture_target) {
  DVLOG(2) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());
  DCHECK_GT(count, 0U);

  if (!gpu_weak_vda_)
    return;

  // TODO(sandersd): VDAs should always be explicit.
  if (pixel_format == PIXEL_FORMAT_UNKNOWN)
    pixel_format = PIXEL_FORMAT_XRGB;

  std::vector<PictureBuffer> picture_buffers =
      picture_buffer_manager_->CreatePictureBuffers(
          count, pixel_format, planes, texture_size, texture_target);
  if (picture_buffers.empty()) {
    parent_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VdaVideoDecoder::EnterErrorState, parent_weak_this_));
    return;
  }

  DCHECK(gpu_weak_vda_);
  vda_->AssignPictureBuffers(std::move(picture_buffers));
}

void VdaVideoDecoder::DismissPictureBuffer(int32_t picture_buffer_id) {
  DVLOG(2) << __func__ << "(" << picture_buffer_id << ")";
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  if (!picture_buffer_manager_->DismissPictureBuffer(picture_buffer_id)) {
    parent_task_runner_->PostTask(
        FROM_HERE,
        base::BindOnce(&VdaVideoDecoder::EnterErrorState, parent_weak_this_));
    return;
  }
}

void VdaVideoDecoder::PictureReady(const Picture& picture) {
  DVLOG(3) << __func__ << "(" << picture.picture_buffer_id() << ")";

  if (parent_task_runner_->BelongsToCurrentThread()) {
    // Note: This optimization is only correct if the output callback does not
    // reentrantly call Decode(). MojoVideoDecoderService is safe, but there is
    // no guarantee in the media::VideoDecoder interface definition.
    PictureReadyOnParentThread(picture);
    return;
  }

  parent_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VdaVideoDecoder::PictureReadyOnParentThread,
                                parent_weak_this_, picture));
}

void VdaVideoDecoder::PictureReadyOnParentThread(Picture picture) {
  DVLOG(3) << __func__ << "(" << picture.picture_buffer_id() << ")";
  DCHECK(parent_task_runner_->BelongsToCurrentThread());

  if (has_error_)
    return;

  // Substitute the container's visible rect if the VDA didn't specify one.
  gfx::Rect visible_rect = picture.visible_rect();
  if (visible_rect.IsEmpty())
    visible_rect = config_.visible_rect();

  // Look up the decode timestamp.
  int32_t bitstream_buffer_id = picture.bitstream_buffer_id();
  const auto timestamp_it = timestamps_.Peek(bitstream_buffer_id);
  if (timestamp_it == timestamps_.end()) {
    DVLOG(1) << "Unknown bitstream buffer " << bitstream_buffer_id;
    EnterErrorState();
    return;
  }

  // Create a VideoFrame for the picture.
  scoped_refptr<VideoFrame> frame = picture_buffer_manager_->CreateVideoFrame(
      picture, timestamp_it->second, visible_rect, config_.natural_size());
  if (!frame) {
    EnterErrorState();
    return;
  }

  output_cb_.Run(std::move(frame));
}

void VdaVideoDecoder::NotifyEndOfBitstreamBuffer(int32_t bitstream_buffer_id) {
  DVLOG(3) << __func__ << "(" << bitstream_buffer_id << ")";

  if (parent_task_runner_->BelongsToCurrentThread()) {
    // Note: This optimization is only correct if the decode callback does not
    // reentrantly call Decode(). MojoVideoDecoderService is safe, but there is
    // no guarantee in the media::VideoDecoder interface definition.
    NotifyEndOfBitstreamBufferOnParentThread(bitstream_buffer_id);
    return;
  }

  parent_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VdaVideoDecoder::NotifyEndOfBitstreamBufferOnParentThread,
                     parent_weak_this_, bitstream_buffer_id));
}

void VdaVideoDecoder::NotifyEndOfBitstreamBufferOnParentThread(
    int32_t bitstream_buffer_id) {
  DVLOG(3) << __func__ << "(" << bitstream_buffer_id << ")";
  DCHECK(parent_task_runner_->BelongsToCurrentThread());

  if (has_error_)
    return;

  // Look up the decode callback.
  const auto decode_cb_it = decode_cbs_.find(bitstream_buffer_id);
  if (decode_cb_it == decode_cbs_.end()) {
    DLOG(ERROR) << "Unknown bitstream buffer " << bitstream_buffer_id;
    EnterErrorState();
    return;
  }

  // Run a local copy in case the decode callback modifies |decode_cbs_|.
  DecodeCB decode_cb = decode_cb_it->second;
  decode_cbs_.erase(decode_cb_it);
  decode_cb.Run(DecodeStatus::OK);
}

void VdaVideoDecoder::NotifyFlushDone() {
  DVLOG(2) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  parent_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VdaVideoDecoder::NotifyFlushDoneOnParentThread,
                                parent_weak_this_));
}

void VdaVideoDecoder::NotifyFlushDoneOnParentThread() {
  DVLOG(2) << __func__;
  DCHECK(parent_task_runner_->BelongsToCurrentThread());

  if (has_error_)
    return;

  DCHECK(decode_cbs_.empty());
  base::ResetAndReturn(&flush_cb_).Run(DecodeStatus::OK);
}

void VdaVideoDecoder::NotifyResetDone() {
  DVLOG(2) << __func__;
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  parent_task_runner_->PostTask(
      FROM_HERE, base::BindOnce(&VdaVideoDecoder::NotifyResetDoneOnParentThread,
                                parent_weak_this_));
}

void VdaVideoDecoder::NotifyResetDoneOnParentThread() {
  DVLOG(2) << __func__;
  DCHECK(parent_task_runner_->BelongsToCurrentThread());

  if (has_error_)
    return;

  // If NotifyFlushDone() has not been called yet, it never will be.
  //
  // We use an on-stack WeakPtr to detect Destroy() being called. A correct
  // client should not call Decode() or Reset() while there is a reset pending,
  // but we should handle that safely as well.
  //
  // TODO(sandersd): This is similar to DestroyCallbacks(); see about merging
  // them.
  base::WeakPtr<VdaVideoDecoder> weak_this = parent_weak_this_;

  std::map<int32_t, DecodeCB> local_decode_cbs = decode_cbs_;
  decode_cbs_.clear();
  for (const auto& it : local_decode_cbs) {
    it.second.Run(DecodeStatus::ABORTED);
    if (!weak_this)
      return;
  }

  if (weak_this && !flush_cb_.is_null())
    base::ResetAndReturn(&flush_cb_).Run(DecodeStatus::ABORTED);

  if (weak_this)
    base::ResetAndReturn(&reset_cb_).Run();
}

void VdaVideoDecoder::NotifyError(VideoDecodeAccelerator::Error error) {
  DVLOG(1) << __func__ << "(" << error << ")";
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  // Invalidate |gpu_weak_vda_| so that we won't make any more |vda_| calls.
  gpu_weak_vda_factory_ = nullptr;

  parent_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VdaVideoDecoder::EnterErrorState, parent_weak_this_));
}

void VdaVideoDecoder::ReusePictureBuffer(int32_t picture_buffer_id) {
  DVLOG(3) << __func__ << "(" << picture_buffer_id << ")";
  DCHECK(gpu_task_runner_->BelongsToCurrentThread());

  if (!gpu_weak_vda_)
    return;

  vda_->ReusePictureBuffer(picture_buffer_id);
}

void VdaVideoDecoder::EnterErrorState() {
  DVLOG(1) << __func__;
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  DCHECK(parent_weak_this_);

  if (has_error_)
    return;

  // Start rejecting client calls immediately.
  has_error_ = true;

  // Destroy callbacks aynchronously to avoid calling them on a client stack.
  parent_task_runner_->PostTask(
      FROM_HERE,
      base::BindOnce(&VdaVideoDecoder::DestroyCallbacks, parent_weak_this_));
}

void VdaVideoDecoder::DestroyCallbacks() {
  DVLOG(3) << __func__;
  DCHECK(parent_task_runner_->BelongsToCurrentThread());
  DCHECK(parent_weak_this_);
  DCHECK(has_error_);

  // We use an on-stack WeakPtr to detect Destroy() being called. Note that any
  // calls to Initialize(), Decode(), or Reset() are asynchronously rejected
  // when |has_error_| is set.
  base::WeakPtr<VdaVideoDecoder> weak_this = parent_weak_this_;

  std::map<int32_t, DecodeCB> local_decode_cbs = decode_cbs_;
  decode_cbs_.clear();
  for (const auto& it : local_decode_cbs) {
    it.second.Run(DecodeStatus::DECODE_ERROR);
    if (!weak_this)
      return;
  }

  if (weak_this && !flush_cb_.is_null())
    base::ResetAndReturn(&flush_cb_).Run(DecodeStatus::DECODE_ERROR);

  // Note: |reset_cb_| cannot return failure, so the client won't actually find
  // out about the error until another operation is attempted.
  if (weak_this && !reset_cb_.is_null())
    base::ResetAndReturn(&reset_cb_).Run();

  if (weak_this && !init_cb_.is_null())
    base::ResetAndReturn(&init_cb_).Run(false);
}

}  // namespace media
