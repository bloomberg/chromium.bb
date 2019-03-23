// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_RENDERER_MEDIA_WEBRTC_RTC_VIDEO_DECODER_ADAPTER_H_
#define CONTENT_RENDERER_MEDIA_WEBRTC_RTC_VIDEO_DECODER_ADAPTER_H_

#include <memory>

#include "base/callback_forward.h"
#include "base/containers/circular_deque.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/synchronization/lock.h"
#include "base/threading/thread_checker.h"
#include "content/common/content_export.h"
#include "media/base/decode_status.h"
#include "media/base/video_codecs.h"
#include "media/base/video_decoder.h"
#include "media/base/video_decoder_config.h"
#include "third_party/webrtc/api/video_codecs/sdp_video_format.h"
#include "third_party/webrtc/modules/video_coding/include/video_codec_interface.h"
#include "ui/gfx/geometry/size.h"

namespace base {
class SingleThreadTaskRunner;
}  // namespace base

namespace media {
class DecoderBuffer;
class GpuVideoAcceleratorFactories;
class MediaLog;
class VideoFrame;
}  // namespace media

namespace content {

// This class decodes video for WebRTC using a media::VideoDecoder. In
// particular, either GpuVideoDecoder or MojoVideoDecoder are used to provide
// access to hardware decoding in the GPU process.
//
// Lifecycle methods are called on the WebRTC worker thread. Decoding happens on
// a WebRTC DecodingThread, which is an rtc::PlatformThread owend by WebRTC; it
// does not have a TaskRunner.
//
// To stop decoding, WebRTC stops the DecodingThread and then calls Release() on
// the worker. Calling the DecodedImageCallback after the DecodingThread is
// stopped is illegal but, because we decode on the media thread, there is no
// way to synchronize this correctly.
class CONTENT_EXPORT RTCVideoDecoderAdapter : public webrtc::VideoDecoder {
 public:
  // Creates and initializes an RTCVideoDecoderAdapter. Returns nullptr if
  // |format| cannot be supported.
  // Called on the worker thread.
  static std::unique_ptr<RTCVideoDecoderAdapter> Create(
      media::GpuVideoAcceleratorFactories* gpu_factories,
      const webrtc::SdpVideoFormat& format);

  // Called on |media_task_runner_|.
  ~RTCVideoDecoderAdapter() override;

  // webrtc::VideoDecoder implementation.
  // Called on the DecodingThread.
  int32_t InitDecode(const webrtc::VideoCodec* codec_settings,
                     int32_t number_of_cores) override;
  // Called on the DecodingThread.
  int32_t RegisterDecodeCompleteCallback(
      webrtc::DecodedImageCallback* callback) override;
  // Called on the DecodingThread.
  int32_t Decode(const webrtc::EncodedImage& input_image,
                 bool missing_frames,
                 const webrtc::CodecSpecificInfo* codec_specific_info,
                 int64_t render_time_ms) override;
  // Called on the worker thread and on the DecodingThread.
  int32_t Release() override;
  // Called on the worker thread and on the DecodingThread.
  const char* ImplementationName() const override;

 private:
  using CreateVideoDecoderCB =
      base::RepeatingCallback<std::unique_ptr<media::VideoDecoder>(
          media::MediaLog*)>;
  using FlushDoneCB = base::OnceCallback<void()>;

  // Called on the worker thread.
  RTCVideoDecoderAdapter(media::GpuVideoAcceleratorFactories* gpu_factories,
                         const media::VideoDecoderConfig& config,
                         const webrtc::SdpVideoFormat& format);

  bool InitializeSync(const media::VideoDecoderConfig& config);
  void InitializeOnMediaThread(const media::VideoDecoderConfig& config,
                               const media::VideoDecoder::InitCB& init_cb);
  void DecodeOnMediaThread();
  void OnDecodeDone(media::DecodeStatus status);
  void OnOutput(const scoped_refptr<media::VideoFrame>& frame);

  bool ShouldReinitializeForSettingHDRColorSpace(
      const webrtc::EncodedImage& input_image) const;
  bool ReinitializeSync(const media::VideoDecoderConfig& config);
  void FlushOnMediaThread(FlushDoneCB flush_success_cb,
                          FlushDoneCB flush_fail_cb);

  // Construction parameters.
  scoped_refptr<base::SingleThreadTaskRunner> media_task_runner_;
  media::GpuVideoAcceleratorFactories* gpu_factories_;
  webrtc::SdpVideoFormat format_;
  media::VideoDecoderConfig config_;

  // Media thread members.
  // |media_log_| must outlive |video_decoder_| because it is passed as a raw
  // pointer.
  std::unique_ptr<media::MediaLog> media_log_;
  std::unique_ptr<media::VideoDecoder> video_decoder_;
  int32_t outstanding_decode_requests_ = 0;

  // Decoding thread members.
  bool key_frame_required_ = true;
  // Shared members.
  base::Lock lock_;
  int32_t consecutive_error_count_ = 0;
  bool has_error_ = false;
  webrtc::DecodedImageCallback* decode_complete_callback_ = nullptr;
  // Requests that have not been submitted to the decoder yet.
  base::circular_deque<scoped_refptr<media::DecoderBuffer>> pending_buffers_;
  // Record of timestamps that have been sent to be decoded. Removing a
  // timestamp will cause the frame to be dropped when it is output.
  base::circular_deque<base::TimeDelta> decode_timestamps_;

  // Thread management.
  THREAD_CHECKER(worker_thread_checker_);
  THREAD_CHECKER(decoding_thread_checker_);

  base::WeakPtr<RTCVideoDecoderAdapter> weak_this_;
  base::WeakPtrFactory<RTCVideoDecoderAdapter> weak_this_factory_;

  DISALLOW_COPY_AND_ASSIGN(RTCVideoDecoderAdapter);
};

}  // namespace content

#endif  // CONTENT_RENDERER_MEDIA_WEBRTC_RTC_VIDEO_DECODER_ADAPTER_H_
