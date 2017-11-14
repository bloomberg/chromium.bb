// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/filters/vpx_video_decoder.h"

#include <stddef.h>
#include <stdint.h>

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/command_line.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/memory/ptr_util.h"
#include "base/metrics/histogram_macros.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/stl_util.h"
#include "base/strings/string_number_conversions.h"
#include "base/sys_byteorder.h"
#include "base/sys_info.h"
#include "base/task_scheduler/post_task.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/threading/thread.h"
#include "base/time/default_tick_clock.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/memory_dump_manager.h"
#include "base/trace_event/memory_dump_provider.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/trace_event.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/decoder_buffer.h"
#include "media/base/media_switches.h"

// Include libvpx header files.
// VPX_CODEC_DISABLE_COMPAT excludes parts of the libvpx API that provide
// backwards compatibility for legacy applications using the library.
#define VPX_CODEC_DISABLE_COMPAT 1
extern "C" {
#include "third_party/libvpx/source/libvpx/vpx/vp8dx.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_decoder.h"
#include "third_party/libvpx/source/libvpx/vpx/vpx_frame_buffer.h"
}

#include "third_party/libyuv/include/libyuv/convert.h"
#include "third_party/libyuv/include/libyuv/planar_functions.h"

namespace media {

constexpr base::TimeDelta kStaleFrameLimit = base::TimeDelta::FromSeconds(10);

// Always try to use three threads for video decoding.  There is little reason
// not to since current day CPUs tend to be multi-core and we measured
// performance benefits on older machines such as P4s with hyperthreading.
static const int kDecodeThreads = 2;
static const int kMaxDecodeThreads = 32;

// Returns the number of threads.
static int GetThreadCount(const VideoDecoderConfig& config) {
  // Refer to http://crbug.com/93932 for tsan suppressions on decoding.
  int decode_threads = kDecodeThreads;

  const base::CommandLine* cmd_line = base::CommandLine::ForCurrentProcess();
  std::string threads(cmd_line->GetSwitchValueASCII(switches::kVideoThreads));
  if (threads.empty() || !base::StringToInt(threads, &decode_threads)) {
    if (config.codec() == kCodecVP9) {
      // For VP9 decode when using the default thread count, increase the number
      // of decode threads to equal the maximum number of tiles possible for
      // higher resolution streams.
      const int width = config.coded_size().width();
      if (width >= 8192)
        decode_threads = 32;
      else if (width >= 4096)
        decode_threads = 16;
      else if (width >= 2048)
        decode_threads = 8;
      else if (width >= 1024)
        decode_threads = 4;
    }

    decode_threads = std::min(decode_threads,
                              base::SysInfo::NumberOfProcessors());
    return decode_threads;
  }

  decode_threads = std::max(decode_threads, 0);
  decode_threads = std::min(decode_threads, kMaxDecodeThreads);
  return decode_threads;
}

static std::unique_ptr<vpx_codec_ctx> InitializeVpxContext(
    const VideoDecoderConfig& config) {
  auto context = std::make_unique<vpx_codec_ctx>();
  vpx_codec_dec_cfg_t vpx_config = {0};
  vpx_config.w = config.coded_size().width();
  vpx_config.h = config.coded_size().height();
  vpx_config.threads = GetThreadCount(config);

  vpx_codec_err_t status = vpx_codec_dec_init(
      context.get(),
      config.codec() == kCodecVP9 ? vpx_codec_vp9_dx() : vpx_codec_vp8_dx(),
      &vpx_config, 0 /* flags */);
  if (status == VPX_CODEC_OK)
    return context;

  DLOG(ERROR) << "vpx_codec_dec_init() failed: "
              << vpx_codec_error(context.get());
  return nullptr;
}

static void DestroyHelper(std::unique_ptr<vpx_codec_ctx> vpx_codec,
                          std::unique_ptr<vpx_codec_ctx> vpx_codec_alpha) {
  // Note: The vpx_codec_destroy() calls below don't release the memory
  // allocated for vpx_codec_ctx, they just release internal allocations, so we
  // still need std::unique_ptr to release the structure memory.
  if (vpx_codec)
    vpx_codec_destroy(vpx_codec.get());

  if (vpx_codec_alpha)
    vpx_codec_destroy(vpx_codec_alpha.get());
}

// MemoryPool is a pool of simple CPU memory, allocated by hand and used by both
// VP9 and any data consumers. This class needs to be ref-counted to hold on to
// allocated memory via the memory-release callback of CreateFrameCallback().
class VpxVideoDecoder::MemoryPool
    : public base::RefCountedThreadSafe<VpxVideoDecoder::MemoryPool>,
      public base::trace_event::MemoryDumpProvider {
 public:
  MemoryPool();

  // Callback that will be called by libvpx when it needs a frame buffer.
  // Parameters:
  // |user_priv|  Private data passed to libvpx (pointer to memory pool).
  // |min_size|   Minimum size needed by libvpx to decompress the next frame.
  // |fb|         Pointer to the frame buffer to update.
  // Returns 0 on success. Returns < 0 on failure.
  static int32_t GetVP9FrameBuffer(void* user_priv,
                                   size_t min_size,
                                   vpx_codec_frame_buffer* fb);

  // Callback that will be called by libvpx when the frame buffer is no longer
  // being used by libvpx. Can be called with NULL user data when decode stops
  // because of an invalid bitstream.
  // Parameters:
  // |user_priv|  Private data passed to libvpx (pointer to memory pool).
  // |fb|         Pointer to the frame buffer that's being released.
  // Returns 0 on success. Returns < 0 on failure.
  static int32_t ReleaseVP9FrameBuffer(void* user_priv,
                                       vpx_codec_frame_buffer* fb);

  // Generates a "no_longer_needed" closure that holds a reference to this pool.
  base::Closure CreateFrameCallback(void* fb_priv_data);

  // base::MemoryDumpProvider.
  bool OnMemoryDump(const base::trace_event::MemoryDumpArgs& args,
                    base::trace_event::ProcessMemoryDump* pmd) override;

  void Shutdown();

  // Reference counted frame buffers used for VP9 decoding.
  struct VP9FrameBuffer {
    std::vector<uint8_t> data;
    std::vector<uint8_t> alpha_data;
    bool held_by_libvpx = false;
    // Needs to be a counter since libvpx may vend a framebuffer multiple times.
    int held_by_frame = 0;
    base::TimeTicks last_use_time;
  };

  size_t get_pool_size_for_testing() const { return frame_buffers_.size(); }

  void set_tick_clock_for_testing(base::TickClock* tick_clock) {
    tick_clock_ = tick_clock;
  }

 private:
  friend class base::RefCountedThreadSafe<VpxVideoDecoder::MemoryPool>;
  ~MemoryPool() override;

  static bool IsUsed(const VP9FrameBuffer* buf);

  // Drop all entries in |frame_buffers_| that report !IsUsed().
  void EraseUnusedResources();

  // Gets the next available frame buffer for use by libvpx.
  VP9FrameBuffer* GetFreeFrameBuffer(size_t min_size);

  // Method that gets called when a VideoFrame that references this pool gets
  // destroyed.
  void OnVideoFrameDestroyed(
      scoped_refptr<base::SequencedTaskRunner> task_runner,
      VP9FrameBuffer* frame_buffer);

  // Frame buffers to be used by libvpx for VP9 Decoding.
  std::vector<std::unique_ptr<VP9FrameBuffer>> frame_buffers_;

  bool in_shutdown_ = false;

  bool registered_dump_provider_ = false;

  // |tick_clock_| is always &|default_tick_clock_| outside of testing.
  base::DefaultTickClock default_tick_clock_;
  base::TickClock* tick_clock_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(MemoryPool);
};

VpxVideoDecoder::MemoryPool::MemoryPool() : tick_clock_(&default_tick_clock_) {
  DETACH_FROM_SEQUENCE(sequence_checker_);
}

VpxVideoDecoder::MemoryPool::~MemoryPool() {
  DCHECK(in_shutdown_);

  // May be destructed on any thread.
}

VpxVideoDecoder::MemoryPool::VP9FrameBuffer*
VpxVideoDecoder::MemoryPool::GetFreeFrameBuffer(size_t min_size) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK(!in_shutdown_);

  if (!registered_dump_provider_) {
    base::trace_event::MemoryDumpManager::GetInstance()
        ->RegisterDumpProviderWithSequencedTaskRunner(
            this, "VpxVideoDecoder", base::SequencedTaskRunnerHandle::Get(),
            MemoryDumpProvider::Options());
    registered_dump_provider_ = true;
  }

  // Check if a free frame buffer exists.
  size_t i = 0;
  for (; i < frame_buffers_.size(); ++i) {
    if (!IsUsed(frame_buffers_[i].get()))
      break;
  }

  if (i == frame_buffers_.size()) {
    // Create a new frame buffer.
    frame_buffers_.push_back(base::MakeUnique<VP9FrameBuffer>());
  }

  // Resize the frame buffer if necessary.
  if (frame_buffers_[i]->data.size() < min_size)
    frame_buffers_[i]->data.resize(min_size);
  return frame_buffers_[i].get();
}

int32_t VpxVideoDecoder::MemoryPool::GetVP9FrameBuffer(
    void* user_priv,
    size_t min_size,
    vpx_codec_frame_buffer* fb) {
  DCHECK(user_priv);
  DCHECK(fb);

  VpxVideoDecoder::MemoryPool* memory_pool =
      static_cast<VpxVideoDecoder::MemoryPool*>(user_priv);

  VP9FrameBuffer* fb_to_use = memory_pool->GetFreeFrameBuffer(min_size);
  if (!fb_to_use)
    return -1;

  fb->data = &fb_to_use->data[0];
  fb->size = fb_to_use->data.size();

  DCHECK(!IsUsed(fb_to_use));
  fb_to_use->held_by_libvpx = true;

  // Set the frame buffer's private data to point at the external frame buffer.
  fb->priv = static_cast<void*>(fb_to_use);
  return 0;
}

int32_t VpxVideoDecoder::MemoryPool::ReleaseVP9FrameBuffer(
    void* user_priv,
    vpx_codec_frame_buffer* fb) {
  DCHECK(user_priv);
  DCHECK(fb);

  if (!fb->priv)
    return -1;

  // Note: libvpx may invoke this method multiple times for the same frame, so
  // we can't DCHECK that |held_by_libvpx| is true.
  VP9FrameBuffer* frame_buffer = static_cast<VP9FrameBuffer*>(fb->priv);
  frame_buffer->held_by_libvpx = false;

  if (!IsUsed(frame_buffer)) {
    // TODO(dalecurtis): This should be |tick_clock_| but we don't have access
    // to the main class from this static function and its only needed for tests
    // which all hit the OnVideoFrameDestroyed() path below instead.
    frame_buffer->last_use_time = base::TimeTicks::Now();
  }

  return 0;
}

base::Closure VpxVideoDecoder::MemoryPool::CreateFrameCallback(
    void* fb_priv_data) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  VP9FrameBuffer* frame_buffer = static_cast<VP9FrameBuffer*>(fb_priv_data);
  ++frame_buffer->held_by_frame;

  return base::Bind(&MemoryPool::OnVideoFrameDestroyed, this,
                    base::SequencedTaskRunnerHandle::Get(), frame_buffer);
}

bool VpxVideoDecoder::MemoryPool::OnMemoryDump(
    const base::trace_event::MemoryDumpArgs& args,
    base::trace_event::ProcessMemoryDump* pmd) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);

  base::trace_event::MemoryAllocatorDump* memory_dump =
      pmd->CreateAllocatorDump("media/vpx/memory_pool");
  base::trace_event::MemoryAllocatorDump* used_memory_dump =
      pmd->CreateAllocatorDump("media/vpx/memory_pool/used");

  pmd->AddSuballocation(memory_dump->guid(),
                        base::trace_event::MemoryDumpManager::GetInstance()
                            ->system_allocator_pool_name());
  size_t bytes_used = 0;
  size_t bytes_reserved = 0;
  for (const auto& frame_buffer : frame_buffers_) {
    if (IsUsed(frame_buffer.get()))
      bytes_used += frame_buffer->data.size();
    bytes_reserved += frame_buffer->data.size();
  }

  memory_dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                         base::trace_event::MemoryAllocatorDump::kUnitsBytes,
                         bytes_reserved);
  used_memory_dump->AddScalar(
      base::trace_event::MemoryAllocatorDump::kNameSize,
      base::trace_event::MemoryAllocatorDump::kUnitsBytes, bytes_used);

  return true;
}

void VpxVideoDecoder::MemoryPool::Shutdown() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  in_shutdown_ = true;

  if (registered_dump_provider_) {
    base::trace_event::MemoryDumpManager::GetInstance()->UnregisterDumpProvider(
        this);
  }

  // Clear any refs held by libvpx which isn't good about cleaning up after
  // itself. This is safe since libvpx has already been shutdown by this point.
  for (const auto& frame_buffer : frame_buffers_)
    frame_buffer->held_by_libvpx = false;

  EraseUnusedResources();
}

// static
bool VpxVideoDecoder::MemoryPool::IsUsed(const VP9FrameBuffer* buf) {
  return buf->held_by_libvpx || buf->held_by_frame > 0;
}

void VpxVideoDecoder::MemoryPool::EraseUnusedResources() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  base::EraseIf(frame_buffers_, [](const std::unique_ptr<VP9FrameBuffer>& buf) {
    return !IsUsed(buf.get());
  });
}

void VpxVideoDecoder::MemoryPool::OnVideoFrameDestroyed(
    scoped_refptr<base::SequencedTaskRunner> task_runner,
    VP9FrameBuffer* frame_buffer) {
  if (!task_runner->RunsTasksInCurrentSequence()) {
    task_runner->PostTask(FROM_HERE,
                          base::Bind(&MemoryPool::OnVideoFrameDestroyed, this,
                                     task_runner, frame_buffer));
    return;
  }

  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  DCHECK_GT(frame_buffer->held_by_frame, 0);
  --frame_buffer->held_by_frame;

  if (in_shutdown_) {
    // If we're in shutdown we can be sure that libvpx has been destroyed.
    EraseUnusedResources();
    return;
  }

  const base::TimeTicks now = tick_clock_->NowTicks();
  if (!IsUsed(frame_buffer))
    frame_buffer->last_use_time = now;

  base::EraseIf(frame_buffers_,
                [now](const std::unique_ptr<VP9FrameBuffer>& buf) {
                  return !IsUsed(buf.get()) &&
                         now - buf->last_use_time > kStaleFrameLimit;
                });
}

VpxVideoDecoder::VpxVideoDecoder()
    : state_(kUninitialized),
      vpx_codec_(nullptr),
      vpx_codec_alpha_(nullptr),
      weak_factory_(this) {
  thread_checker_.DetachFromThread();
}

VpxVideoDecoder::~VpxVideoDecoder() {
  DCHECK(thread_checker_.CalledOnValidThread());
  CloseDecoder();
}

std::string VpxVideoDecoder::GetDisplayName() const {
  return "VpxVideoDecoder";
}

void VpxVideoDecoder::Initialize(const VideoDecoderConfig& config,
                                 bool /* low_delay */,
                                 CdmContext* /* cdm_context */,
                                 const InitCB& init_cb,
                                 const OutputCB& output_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(config.IsValidConfig());

  InitCB bound_init_cb = BindToCurrentLoop(init_cb);

  if (config.is_encrypted() || !ConfigureDecoder(config)) {
    bound_init_cb.Run(false);
    return;
  }

  // Success!
  config_ = config;
  state_ = kNormal;
  output_cb_ = offload_task_runner_ ? BindToCurrentLoop(output_cb) : output_cb;
  bound_init_cb.Run(true);
}

void VpxVideoDecoder::DecodeBuffer(const scoped_refptr<DecoderBuffer>& buffer,
                                   const DecodeCB& bound_decode_cb) {
  DCHECK_NE(state_, kUninitialized)
      << "Called Decode() before successful Initialize()";

  if (state_ == kError) {
    bound_decode_cb.Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  if (state_ == kDecodeFinished) {
    bound_decode_cb.Run(DecodeStatus::OK);
    return;
  }

  if (state_ == kNormal && buffer->end_of_stream()) {
    state_ = kDecodeFinished;
    bound_decode_cb.Run(DecodeStatus::OK);
    return;
  }

  bool decode_okay;
  scoped_refptr<VideoFrame> video_frame;
  if (config_.codec() == kCodecVP9) {
    SCOPED_UMA_HISTOGRAM_TIMER("Media.VpxVideoDecoder.Vp9DecodeTime");
    decode_okay = VpxDecode(buffer, &video_frame);
  } else {
    decode_okay = VpxDecode(buffer, &video_frame);
  }

  if (!decode_okay) {
    state_ = kError;
    bound_decode_cb.Run(DecodeStatus::DECODE_ERROR);
    return;
  }

  // We might get a successful VpxDecode but not a frame if only a partial
  // decode happened.
  if (video_frame) {
    video_frame->metadata()->SetBoolean(VideoFrameMetadata::POWER_EFFICIENT,
                                        false);
    // Safe to call |output_cb_| here even if we're on the offload thread since
    // it is only set once during Initialize() and never changed.
    output_cb_.Run(video_frame);
  }

  // VideoDecoderShim expects |decode_cb| call after |output_cb_|.
  bound_decode_cb.Run(DecodeStatus::OK);
}

void VpxVideoDecoder::Decode(const scoped_refptr<DecoderBuffer>& buffer,
                             const DecodeCB& decode_cb) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DCHECK(buffer.get());
  DCHECK(!decode_cb.is_null());

  DecodeCB bound_decode_cb = BindToCurrentLoop(decode_cb);

  if (offload_task_runner_) {
    offload_task_runner_->PostTask(
        FROM_HERE, base::Bind(&VpxVideoDecoder::DecodeBuffer,
                              base::Unretained(this), buffer, bound_decode_cb));
  } else {
    DecodeBuffer(buffer, bound_decode_cb);
  }
}

void VpxVideoDecoder::Reset(const base::Closure& closure) {
  DCHECK(thread_checker_.CalledOnValidThread());
  if (offload_task_runner_) {
    offload_task_runner_->PostTask(
        FROM_HERE,
        BindToCurrentLoop(base::Bind(&VpxVideoDecoder::ResetHelper,
                                     weak_factory_.GetWeakPtr(), closure)));
    return;
  }

  // BindToCurrentLoop() to avoid calling |closure| inmediately.
  ResetHelper(BindToCurrentLoop(closure));
}

size_t VpxVideoDecoder::GetPoolSizeForTesting() const {
  return memory_pool_->get_pool_size_for_testing();
}

void VpxVideoDecoder::SetTickClockForTesting(base::TickClock* tick_clock) {
  memory_pool_->set_tick_clock_for_testing(tick_clock);
}

void VpxVideoDecoder::ResetHelper(const base::Closure& closure) {
  DCHECK(thread_checker_.CalledOnValidThread());
  state_ = kNormal;
  closure.Run();
}

bool VpxVideoDecoder::ConfigureDecoder(const VideoDecoderConfig& config) {
  if (config.codec() != kCodecVP8 && config.codec() != kCodecVP9)
    return false;

  // These are the combinations of codec-pixel format supported in principle.
  DCHECK(
      (config.codec() == kCodecVP8 && config.format() == PIXEL_FORMAT_YV12) ||
      (config.codec() == kCodecVP8 && config.format() == PIXEL_FORMAT_YV12A) ||
      (config.codec() == kCodecVP9 && config.format() == PIXEL_FORMAT_YV12) ||
      (config.codec() == kCodecVP9 && config.format() == PIXEL_FORMAT_YV12A) ||
      (config.codec() == kCodecVP9 && config.format() == PIXEL_FORMAT_YV24));

#if !defined(DISABLE_FFMPEG_VIDEO_DECODERS)
  // When FFmpegVideoDecoder is available it handles VP8 that doesn't have
  // alpha, and VpxVideoDecoder will handle VP8 with alpha.
  if (config.codec() == kCodecVP8 && config.format() != PIXEL_FORMAT_YV12A)
    return false;
#endif

  CloseDecoder();

  vpx_codec_ = InitializeVpxContext(config);
  if (!vpx_codec_)
    return false;

  // Configure VP9 to decode on our buffers to skip a data copy on
  // decoding. For YV12A-VP9, we use our buffers for the Y, U and V planes and
  // copy the A plane.
  if (config.codec() == kCodecVP9) {
    DCHECK(vpx_codec_get_caps(vpx_codec_->iface) &
           VPX_CODEC_CAP_EXTERNAL_FRAME_BUFFER);

    // Move high resolution vp9 decodes off of the main media thread (otherwise
    // decode may block audio decoding, demuxing, and other control activities).
    if (config.coded_size().width() >= 1024) {
      if (!offload_task_runner_) {
        offload_task_runner_ = base::CreateSequencedTaskRunnerWithTraits(
            {base::TaskPriority::USER_BLOCKING});
      }
    } else {
      offload_task_runner_ = nullptr;
    }

    DCHECK(!memory_pool_);
    memory_pool_ = new MemoryPool();
    if (vpx_codec_set_frame_buffer_functions(
            vpx_codec_.get(), &MemoryPool::GetVP9FrameBuffer,
            &MemoryPool::ReleaseVP9FrameBuffer, memory_pool_.get())) {
      DLOG(ERROR) << "Failed to configure external buffers. "
                  << vpx_codec_error(vpx_codec_.get());
      return false;
    }
  }

  if (config.format() != PIXEL_FORMAT_YV12A)
    return true;

  vpx_codec_alpha_ = InitializeVpxContext(config);
  return !!vpx_codec_alpha_;
}

void VpxVideoDecoder::CloseDecoder() {
  if (offload_task_runner_) {
    offload_task_runner_->PostTask(
        FROM_HERE, base::Bind(&DestroyHelper, base::Passed(&vpx_codec_),
                              base::Passed(&vpx_codec_alpha_)));
  } else {
    DestroyHelper(std::move(vpx_codec_), std::move(vpx_codec_alpha_));
  }

  if (memory_pool_) {
    if (offload_task_runner_) {
      // Shutdown must be called on the same thread as buffers are created.
      offload_task_runner_->PostTask(
          FROM_HERE,
          base::Bind(&VpxVideoDecoder::MemoryPool::Shutdown, memory_pool_));
    } else {
      memory_pool_->Shutdown();
    }

    memory_pool_ = nullptr;
  }

  if (offload_task_runner_) {
    // We must wait for the offload task runner here because it's still active
    // it may be writing to |state_| and reading from |config_|.
    // TODO(dalecurtis): This could be avoided by creating an inner class for
    // decoding that has copies of state and config.
    base::WaitableEvent waiter(base::WaitableEvent::ResetPolicy::AUTOMATIC,
                               base::WaitableEvent::InitialState::NOT_SIGNALED);
    offload_task_runner_->PostTask(
        FROM_HERE,
        base::Bind(&base::WaitableEvent::Signal, base::Unretained(&waiter)));
    waiter.Wait();
  }
}

bool VpxVideoDecoder::VpxDecode(const scoped_refptr<DecoderBuffer>& buffer,
                                scoped_refptr<VideoFrame>* video_frame) {
  DCHECK(video_frame);
  DCHECK(!buffer->end_of_stream());

  int64_t timestamp = buffer->timestamp().InMicroseconds();
  void* user_priv = reinterpret_cast<void*>(&timestamp);
  {
    TRACE_EVENT1("media", "vpx_codec_decode", "timestamp", timestamp);
    vpx_codec_err_t status =
        vpx_codec_decode(vpx_codec_.get(), buffer->data(), buffer->data_size(),
                         user_priv, 0 /* deadline */);
    if (status != VPX_CODEC_OK) {
      DLOG(ERROR) << "vpx_codec_decode() error: "
                  << vpx_codec_err_to_string(status);
      return false;
    }
  }

  // Gets pointer to decoded data.
  vpx_codec_iter_t iter = NULL;
  const vpx_image_t* vpx_image = vpx_codec_get_frame(vpx_codec_.get(), &iter);
  if (!vpx_image) {
    *video_frame = nullptr;
    return true;
  }

  if (vpx_image->user_priv != user_priv) {
    DLOG(ERROR) << "Invalid output timestamp.";
    return false;
  }

  const vpx_image_t* vpx_image_alpha = nullptr;
  AlphaDecodeStatus alpha_decode_status =
      DecodeAlphaPlane(vpx_image, &vpx_image_alpha, buffer);
  if (alpha_decode_status == kAlphaPlaneError) {
    return false;
  } else if (alpha_decode_status == kNoAlphaPlaneData) {
    *video_frame = nullptr;
    return true;
  }
  if (!CopyVpxImageToVideoFrame(vpx_image, vpx_image_alpha, video_frame)) {
    return false;
  }
  if (vpx_image_alpha && config_.codec() == kCodecVP8) {
    libyuv::CopyPlane(vpx_image_alpha->planes[VPX_PLANE_Y],
                      vpx_image_alpha->stride[VPX_PLANE_Y],
                      (*video_frame)->visible_data(VideoFrame::kAPlane),
                      (*video_frame)->stride(VideoFrame::kAPlane),
                      (*video_frame)->visible_rect().width(),
                      (*video_frame)->visible_rect().height());
  }

  (*video_frame)->set_timestamp(base::TimeDelta::FromMicroseconds(timestamp));

  // Default to the color space from the config, but if the bistream specifies
  // one, prefer that instead.
  ColorSpace color_space = config_.color_space();
  if (vpx_image->cs == VPX_CS_BT_709)
    color_space = COLOR_SPACE_HD_REC709;
  else if (vpx_image->cs == VPX_CS_BT_601 || vpx_image->cs == VPX_CS_SMPTE_170)
    color_space = COLOR_SPACE_SD_REC601;
  (*video_frame)
      ->metadata()
      ->SetInteger(VideoFrameMetadata::COLOR_SPACE, color_space);

  if (config_.color_space_info() != VideoColorSpace()) {
    // config_.color_space_info() comes from the color tag which is
    // more expressive than the bitstream, so prefer it over the
    // bitstream data below.
    (*video_frame)
        ->set_color_space(config_.color_space_info().ToGfxColorSpace());
  } else {
    gfx::ColorSpace::PrimaryID primaries = gfx::ColorSpace::PrimaryID::INVALID;
    gfx::ColorSpace::TransferID transfer = gfx::ColorSpace::TransferID::INVALID;
    gfx::ColorSpace::MatrixID matrix = gfx::ColorSpace::MatrixID::INVALID;
    gfx::ColorSpace::RangeID range = vpx_image->range == VPX_CR_FULL_RANGE
                                         ? gfx::ColorSpace::RangeID::FULL
                                         : gfx::ColorSpace::RangeID::LIMITED;

    switch (vpx_image->cs) {
      case VPX_CS_BT_601:
      case VPX_CS_SMPTE_170:
        primaries = gfx::ColorSpace::PrimaryID::SMPTE170M;
        transfer = gfx::ColorSpace::TransferID::SMPTE170M;
        matrix = gfx::ColorSpace::MatrixID::SMPTE170M;
        break;
      case VPX_CS_SMPTE_240:
        primaries = gfx::ColorSpace::PrimaryID::SMPTE240M;
        transfer = gfx::ColorSpace::TransferID::SMPTE240M;
        matrix = gfx::ColorSpace::MatrixID::SMPTE240M;
        break;
      case VPX_CS_BT_709:
        primaries = gfx::ColorSpace::PrimaryID::BT709;
        transfer = gfx::ColorSpace::TransferID::BT709;
        matrix = gfx::ColorSpace::MatrixID::BT709;
        break;
      case VPX_CS_BT_2020:
        primaries = gfx::ColorSpace::PrimaryID::BT2020;
        if (vpx_image->bit_depth >= 12) {
          transfer = gfx::ColorSpace::TransferID::BT2020_12;
        } else if (vpx_image->bit_depth >= 10) {
          transfer = gfx::ColorSpace::TransferID::BT2020_10;
        } else {
          transfer = gfx::ColorSpace::TransferID::BT709;
        }
        matrix = gfx::ColorSpace::MatrixID::BT2020_NCL;  // is this right?
        break;
      case VPX_CS_SRGB:
        primaries = gfx::ColorSpace::PrimaryID::BT709;
        transfer = gfx::ColorSpace::TransferID::IEC61966_2_1;
        matrix = gfx::ColorSpace::MatrixID::BT709;
        break;

      default:
        break;
    }

    // TODO(ccameron): Set a color space even for unspecified values.
    if (primaries != gfx::ColorSpace::PrimaryID::INVALID) {
      (*video_frame)
          ->set_color_space(
              gfx::ColorSpace(primaries, transfer, matrix, range));
    }
  }

  return true;
}

VpxVideoDecoder::AlphaDecodeStatus VpxVideoDecoder::DecodeAlphaPlane(
    const struct vpx_image* vpx_image,
    const struct vpx_image** vpx_image_alpha,
    const scoped_refptr<DecoderBuffer>& buffer) {
  if (!vpx_codec_alpha_ || buffer->side_data_size() < 8) {
    return kAlphaPlaneProcessed;
  }

  // First 8 bytes of side data is |side_data_id| in big endian.
  const uint64_t side_data_id = base::NetToHost64(
      *(reinterpret_cast<const uint64_t*>(buffer->side_data())));
  if (side_data_id != 1) {
    return kAlphaPlaneProcessed;
  }

  // Try and decode buffer->side_data() minus the first 8 bytes as a full
  // frame.
  int64_t timestamp_alpha = buffer->timestamp().InMicroseconds();
  void* user_priv_alpha = reinterpret_cast<void*>(&timestamp_alpha);
  {
    TRACE_EVENT1("media", "vpx_codec_decode_alpha", "timestamp_alpha",
                 timestamp_alpha);
    vpx_codec_err_t status = vpx_codec_decode(
        vpx_codec_alpha_.get(), buffer->side_data() + 8,
        buffer->side_data_size() - 8, user_priv_alpha, 0 /* deadline */);
    if (status != VPX_CODEC_OK) {
      DLOG(ERROR) << "vpx_codec_decode() failed for the alpha: "
                  << vpx_codec_error(vpx_codec_.get());
      return kAlphaPlaneError;
    }
  }

  vpx_codec_iter_t iter_alpha = NULL;
  *vpx_image_alpha = vpx_codec_get_frame(vpx_codec_alpha_.get(), &iter_alpha);
  if (!(*vpx_image_alpha)) {
    return kNoAlphaPlaneData;
  }

  if ((*vpx_image_alpha)->user_priv != user_priv_alpha) {
    DLOG(ERROR) << "Invalid output timestamp on alpha.";
    return kAlphaPlaneError;
  }

  if ((*vpx_image_alpha)->d_h != vpx_image->d_h ||
      (*vpx_image_alpha)->d_w != vpx_image->d_w) {
    DLOG(ERROR) << "The alpha plane dimensions are not the same as the "
                   "image dimensions.";
    return kAlphaPlaneError;
  }

  if (config_.codec() == kCodecVP9) {
    VpxVideoDecoder::MemoryPool::VP9FrameBuffer* frame_buffer =
        static_cast<VpxVideoDecoder::MemoryPool::VP9FrameBuffer*>(
            vpx_image->fb_priv);
    uint64_t alpha_plane_size =
        (*vpx_image_alpha)->stride[VPX_PLANE_Y] * (*vpx_image_alpha)->d_h;
    if (frame_buffer->alpha_data.size() < alpha_plane_size) {
      frame_buffer->alpha_data.resize(alpha_plane_size);
    }
    libyuv::CopyPlane((*vpx_image_alpha)->planes[VPX_PLANE_Y],
                      (*vpx_image_alpha)->stride[VPX_PLANE_Y],
                      &frame_buffer->alpha_data[0],
                      (*vpx_image_alpha)->stride[VPX_PLANE_Y],
                      (*vpx_image_alpha)->d_w, (*vpx_image_alpha)->d_h);
  }
  return kAlphaPlaneProcessed;
}

bool VpxVideoDecoder::CopyVpxImageToVideoFrame(
    const struct vpx_image* vpx_image,
    const struct vpx_image* vpx_image_alpha,
    scoped_refptr<VideoFrame>* video_frame) {
  DCHECK(vpx_image);

  VideoPixelFormat codec_format;
  switch (vpx_image->fmt) {
    case VPX_IMG_FMT_I420:
      codec_format = vpx_image_alpha ? PIXEL_FORMAT_YV12A : PIXEL_FORMAT_YV12;
      break;

    case VPX_IMG_FMT_I444:
      codec_format = PIXEL_FORMAT_YV24;
      break;

    case VPX_IMG_FMT_I42016:
      switch (vpx_image->bit_depth) {
        case 10:
          codec_format = PIXEL_FORMAT_YUV420P10;
          break;
        case 12:
          codec_format = PIXEL_FORMAT_YUV420P12;
          break;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << vpx_image->bit_depth;
          return false;
      }
      break;

    case VPX_IMG_FMT_I42216:
      switch (vpx_image->bit_depth) {
        case 10:
          codec_format = PIXEL_FORMAT_YUV422P10;
          break;
        case 12:
          codec_format = PIXEL_FORMAT_YUV422P12;
          break;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << vpx_image->bit_depth;
          return false;
      }
      break;

    case VPX_IMG_FMT_I44416:
      switch (vpx_image->bit_depth) {
        case 10:
          codec_format = PIXEL_FORMAT_YUV444P10;
          break;
        case 12:
          codec_format = PIXEL_FORMAT_YUV444P12;
          break;
        default:
          DLOG(ERROR) << "Unsupported bit depth: " << vpx_image->bit_depth;
          return false;
      }
      break;

    default:
      DLOG(ERROR) << "Unsupported pixel format: " << vpx_image->fmt;
      return false;
  }

  // The mixed |w|/|d_h| in |coded_size| is intentional. Setting the correct
  // coded width is necessary to allow coalesced memory access, which may avoid
  // frame copies. Setting the correct coded height however does not have any
  // benefit, and only risk copying too much data.
  const gfx::Size coded_size(vpx_image->w, vpx_image->d_h);
  const gfx::Size visible_size(vpx_image->d_w, vpx_image->d_h);

  if (memory_pool_.get()) {
    DCHECK_EQ(kCodecVP9, config_.codec());
    if (vpx_image_alpha) {
      VpxVideoDecoder::MemoryPool::VP9FrameBuffer* frame_buffer =
          static_cast<VpxVideoDecoder::MemoryPool::VP9FrameBuffer*>(
              vpx_image->fb_priv);
      *video_frame = VideoFrame::WrapExternalYuvaData(
          codec_format, coded_size, gfx::Rect(visible_size),
          config_.natural_size(), vpx_image->stride[VPX_PLANE_Y],
          vpx_image->stride[VPX_PLANE_U], vpx_image->stride[VPX_PLANE_V],
          vpx_image_alpha->stride[VPX_PLANE_Y], vpx_image->planes[VPX_PLANE_Y],
          vpx_image->planes[VPX_PLANE_U], vpx_image->planes[VPX_PLANE_V],
          &frame_buffer->alpha_data[0], kNoTimestamp);
    } else {
      *video_frame = VideoFrame::WrapExternalYuvData(
          codec_format, coded_size, gfx::Rect(visible_size),
          config_.natural_size(), vpx_image->stride[VPX_PLANE_Y],
          vpx_image->stride[VPX_PLANE_U], vpx_image->stride[VPX_PLANE_V],
          vpx_image->planes[VPX_PLANE_Y], vpx_image->planes[VPX_PLANE_U],
          vpx_image->planes[VPX_PLANE_V], kNoTimestamp);
    }
    if (!(*video_frame))
      return false;

    video_frame->get()->AddDestructionObserver(
        memory_pool_->CreateFrameCallback(vpx_image->fb_priv));
    return true;
  }

  DCHECK(codec_format == PIXEL_FORMAT_YV12 ||
         codec_format == PIXEL_FORMAT_YV12A);

  *video_frame = frame_pool_.CreateFrame(codec_format, visible_size,
                                         gfx::Rect(visible_size),
                                         config_.natural_size(), kNoTimestamp);
  if (!(*video_frame))
    return false;

  libyuv::I420Copy(
      vpx_image->planes[VPX_PLANE_Y], vpx_image->stride[VPX_PLANE_Y],
      vpx_image->planes[VPX_PLANE_U], vpx_image->stride[VPX_PLANE_U],
      vpx_image->planes[VPX_PLANE_V], vpx_image->stride[VPX_PLANE_V],
      (*video_frame)->visible_data(VideoFrame::kYPlane),
      (*video_frame)->stride(VideoFrame::kYPlane),
      (*video_frame)->visible_data(VideoFrame::kUPlane),
      (*video_frame)->stride(VideoFrame::kUPlane),
      (*video_frame)->visible_data(VideoFrame::kVPlane),
      (*video_frame)->stride(VideoFrame::kVPlane), coded_size.width(),
      coded_size.height());

  return true;
}

}  // namespace media
