// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/video_track_recorder.h"

#include <utility>

#include "base/bind.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/sys_info.h"
#include "base/threading/thread.h"
#include "base/time/time.h"
#include "base/trace_event/trace_event.h"
#include "media/base/video_frame.h"
#include "ui/gfx/geometry/size.h"

extern "C" {
// VPX_CODEC_DISABLE_COMPAT excludes parts of the libvpx API that provide
// backwards compatibility for legacy applications using the library.
#define VPX_CODEC_DISABLE_COMPAT 1
#include "third_party/libvpx_new/source/libvpx/vpx/vp8cx.h"
#include "third_party/libvpx_new/source/libvpx/vpx/vpx_encoder.h"
}

using media::VideoFrame;
using media::VideoFrameMetadata;

namespace content {

namespace {

const vpx_codec_flags_t kNoFlags = 0;

// Originally from remoting/codec/scoped_vpx_codec.h.
// TODO(mcasas): Refactor into a common location.
struct VpxCodecDeleter {
  void operator()(vpx_codec_ctx_t* codec) {
    if (!codec)
      return;
    vpx_codec_err_t ret = vpx_codec_destroy(codec);
    CHECK_EQ(ret, VPX_CODEC_OK);
    delete codec;
  }
};

typedef scoped_ptr<vpx_codec_ctx_t, VpxCodecDeleter> ScopedVpxCodecCtxPtr;

void OnFrameEncodeCompleted(
    const VideoTrackRecorder::OnEncodedVideoCB& on_encoded_video_cb,
    const scoped_refptr<VideoFrame>& frame,
    scoped_ptr<std::string> data,
    base::TimeTicks capture_timestamp,
    bool keyframe) {
  DVLOG(1) << (keyframe ? "" : "non ") << "keyframe "<< data->length() << "B, "
           << capture_timestamp << " ms";
  on_encoded_video_cb.Run(frame, std::move(data), capture_timestamp, keyframe);
}

}  // anonymous namespace

// Inner class encapsulating all libvpx interactions and the encoding+delivery
// of received frames. Limitation: Only VP8 is supported for the time being.
// This class must be ref-counted because the MediaStreamVideoTrack will hold a
// reference to it, via the callback MediaStreamVideoSink passes along, and it's
// unknown when exactly it will release that reference. This class:
// - is created and destroyed on its parent's thread (usually the main Render
// thread);
// - receives VideoFrames and Run()s the callbacks on |origin_task_runner_|,
// which is cached on first frame arrival, and is supposed to be the render IO
// thread, but this is not enforced;
// - uses an internal |encoding_thread_| for libvpx interactions, notably for
// encoding (which might take some time).
class VideoTrackRecorder::VpxEncoder final
    : public base::RefCountedThreadSafe<VpxEncoder> {
 public:
  static void ShutdownEncoder(scoped_ptr<base::Thread> encoding_thread,
                              ScopedVpxCodecCtxPtr encoder);

  VpxEncoder(bool use_vp9, const OnEncodedVideoCB& on_encoded_video_callback);

  void StartFrameEncode(const scoped_refptr<VideoFrame>& frame,
                        base::TimeTicks capture_timestamp);

  void set_paused(bool paused) { paused_ = paused; }

 private:
  friend class base::RefCountedThreadSafe<VpxEncoder>;
  ~VpxEncoder();

  void EncodeOnEncodingThread(const scoped_refptr<VideoFrame>& frame,
                              base::TimeTicks capture_timestamp);

  void ConfigureEncoding(const gfx::Size& size);

  // Returns true if |codec_config_| has been filled in at least once.
  bool IsInitialized() const;

  // Estimate the frame duration from |frame| and |last_frame_timestamp_|.
  base::TimeDelta CalculateFrameDuration(
      const scoped_refptr<VideoFrame>& frame);

  // While |paused_|, frames are not encoded.
  bool paused_;

  // Force usage of VP9 for encoding, instead of VP8 which is the default.
  const bool use_vp9_;

  // Used to shutdown properly on the same thread we were created.
  const scoped_refptr<base::SingleThreadTaskRunner> main_task_runner_;

  // Task runner where frames to encode and reply callbacks must happen.
  scoped_refptr<base::SingleThreadTaskRunner> origin_task_runner_;

  // This callback should be exercised on IO thread.
  const OnEncodedVideoCB on_encoded_video_callback_;

  // Thread for encoding. Active for the lifetime of VpxEncoder. All variables
  // below this are used in this thread.
  scoped_ptr<base::Thread> encoding_thread_;
  // VP8 internal objects: configuration and encoder.
  vpx_codec_enc_cfg_t codec_config_;
  // |encoder_| is a special scoped pointer to guarantee proper destruction.
  // Again, it should only be accessed on |encoding_thread_|.
  ScopedVpxCodecCtxPtr encoder_;

  // The |VideoFrame::timestamp()| of the last encoded frame.  This is used to
  // predict the duration of the next frame.
  base::TimeDelta last_frame_timestamp_;

  DISALLOW_COPY_AND_ASSIGN(VpxEncoder);
};

// static
void VideoTrackRecorder::VpxEncoder::ShutdownEncoder(
    scoped_ptr<base::Thread> encoding_thread,
    ScopedVpxCodecCtxPtr encoder) {
  DCHECK(encoding_thread->IsRunning());
  encoding_thread->Stop();
  // Both |encoding_thread| and |encoder| will be destroyed at end-of-scope.
}

VideoTrackRecorder::VpxEncoder::VpxEncoder(
    bool use_vp9,
    const OnEncodedVideoCB& on_encoded_video_callback)
    : paused_(false),
      use_vp9_(use_vp9),
      main_task_runner_(base::MessageLoop::current()->task_runner()),
      on_encoded_video_callback_(on_encoded_video_callback),
      encoding_thread_(new base::Thread("EncodingThread")) {
  DCHECK(!on_encoded_video_callback_.is_null());

  codec_config_.g_timebase.den = 0;  // Not initialized.

  DCHECK(!encoding_thread_->IsRunning());
  encoding_thread_->Start();
}

VideoTrackRecorder::VpxEncoder::~VpxEncoder() {
  main_task_runner_->PostTask(FROM_HERE,
                              base::Bind(&VpxEncoder::ShutdownEncoder,
                                         base::Passed(&encoding_thread_),
                                         base::Passed(&encoder_)));
}

void VideoTrackRecorder::VpxEncoder::StartFrameEncode(
    const scoped_refptr<VideoFrame>& frame,
    base::TimeTicks capture_timestamp) {
  // Cache the thread sending frames on first frame arrival.
  if (!origin_task_runner_.get())
    origin_task_runner_ = base::MessageLoop::current()->task_runner();
  DCHECK(origin_task_runner_->BelongsToCurrentThread());
  if (paused_)
    return;
  encoding_thread_->task_runner()->PostTask(
      FROM_HERE, base::Bind(&VpxEncoder::EncodeOnEncodingThread,
                            this, frame, capture_timestamp));
}

void VideoTrackRecorder::VpxEncoder::EncodeOnEncodingThread(
    const scoped_refptr<VideoFrame>& frame,
    base::TimeTicks capture_timestamp) {
  TRACE_EVENT0("video",
               "VideoTrackRecorder::VpxEncoder::EncodeOnEncodingThread");
  DCHECK(encoding_thread_->task_runner()->BelongsToCurrentThread());

  const gfx::Size frame_size = frame->visible_rect().size();
  if (!IsInitialized() ||
      gfx::Size(codec_config_.g_w, codec_config_.g_h) != frame_size) {
    ConfigureEncoding(frame_size);
  }

  vpx_image_t vpx_image;
  vpx_image_t* const result = vpx_img_wrap(&vpx_image,
                                           VPX_IMG_FMT_I420,
                                           frame_size.width(),
                                           frame_size.height(),
                                           1  /* align */,
                                           frame->data(VideoFrame::kYPlane));
  DCHECK_EQ(result, &vpx_image);
  vpx_image.planes[VPX_PLANE_Y] = frame->visible_data(VideoFrame::kYPlane);
  vpx_image.planes[VPX_PLANE_U] = frame->visible_data(VideoFrame::kUPlane);
  vpx_image.planes[VPX_PLANE_V] = frame->visible_data(VideoFrame::kVPlane);
  vpx_image.stride[VPX_PLANE_Y] = frame->stride(VideoFrame::kYPlane);
  vpx_image.stride[VPX_PLANE_U] = frame->stride(VideoFrame::kUPlane);
  vpx_image.stride[VPX_PLANE_V] = frame->stride(VideoFrame::kVPlane);

  const base::TimeDelta duration = CalculateFrameDuration(frame);
  // Encode the frame.  The presentation time stamp argument here is fixed to
  // zero to force the encoder to base its single-frame bandwidth calculations
  // entirely on |predicted_frame_duration|.
  const vpx_codec_err_t ret = vpx_codec_encode(encoder_.get(),
                                               &vpx_image,
                                               0  /* pts */,
                                               duration.InMicroseconds(),
                                               kNoFlags,
                                               VPX_DL_REALTIME);
  DCHECK_EQ(ret, VPX_CODEC_OK) << vpx_codec_err_to_string(ret) << ", #"
                               << vpx_codec_error(encoder_.get()) << " -"
                               << vpx_codec_error_detail(encoder_.get());

  scoped_ptr<std::string> data(new std::string);
  bool keyframe = false;
  vpx_codec_iter_t iter = NULL;
  const vpx_codec_cx_pkt_t* pkt = NULL;
  while ((pkt = vpx_codec_get_cx_data(encoder_.get(), &iter)) != NULL) {
    if (pkt->kind != VPX_CODEC_CX_FRAME_PKT)
      continue;
    data->assign(static_cast<char*>(pkt->data.frame.buf), pkt->data.frame.sz);
    keyframe = (pkt->data.frame.flags & VPX_FRAME_IS_KEY) != 0;
    break;
  }
  origin_task_runner_->PostTask(FROM_HERE,
      base::Bind(OnFrameEncodeCompleted,
                 on_encoded_video_callback_,
                 frame,
                 base::Passed(&data),
                 capture_timestamp,
                 keyframe));
}

void VideoTrackRecorder::VpxEncoder::ConfigureEncoding(const gfx::Size& size) {
  if (IsInitialized()) {
    // TODO(mcasas) VP8 quirk/optimisation: If the new |size| is strictly less-
    // than-or-equal than the old size, in terms of area, the existing encoder
    // instance could be reused after changing |codec_config_.{g_w,g_h}|.
    DVLOG(1) << "Destroying/Re-Creating encoder for new frame size: "
             << gfx::Size(codec_config_.g_w, codec_config_.g_h).ToString()
             << " --> " << size.ToString() << (use_vp9_ ? " vp9" : " vp8");
    encoder_.reset();
  }

  const vpx_codec_iface_t* interface =
      use_vp9_ ? vpx_codec_vp9_cx() : vpx_codec_vp8_cx();
  const vpx_codec_err_t result = vpx_codec_enc_config_default(interface,
                                                              &codec_config_,
                                                              0 /* reserved */);
  DCHECK_EQ(VPX_CODEC_OK, result);

  // Adjust default bit rate to account for the actual size.
  DCHECK_EQ(320u, codec_config_.g_w);
  DCHECK_EQ(240u, codec_config_.g_h);
  DCHECK_EQ(256u, codec_config_.rc_target_bitrate);
  codec_config_.rc_target_bitrate = size.GetArea() *
                                    codec_config_.rc_target_bitrate /
                                    codec_config_.g_w / codec_config_.g_h;
  // Both VP8/VP9 configuration should be Variable BitRate by default.
  DCHECK_EQ(VPX_VBR, codec_config_.rc_end_usage);
  if (use_vp9_) {
    // Number of frames to consume before producing output.
    codec_config_.g_lag_in_frames = 0;

    // DCHECK that the profile selected by default is I420 (magic number 0).
    DCHECK_EQ(0u, codec_config_.g_profile);
  } else {
    // VP8 always produces frames instantaneously.
    DCHECK_EQ(0u, codec_config_.g_lag_in_frames);
  }

  DCHECK(size.width());
  DCHECK(size.height());
  codec_config_.g_w = size.width();
  codec_config_.g_h = size.height();
  codec_config_.g_pass = VPX_RC_ONE_PASS;

  // Timebase is the smallest interval used by the stream, can be set to the
  // frame rate or to e.g. microseconds.
  codec_config_.g_timebase.num = 1;
  codec_config_.g_timebase.den = base::Time::kMicrosecondsPerSecond;

  // Let the encoder decide where to place the Keyframes, between min and max.
  // In VPX_KF_AUTO mode libvpx will sometimes emit keyframes regardless of min/
  // max distance out of necessity.
  // Note that due to http://crbug.com/440223, it might be necessary to force a
  // key frame after 10,000frames since decoding fails after 30,000 non-key
  // frames.
  codec_config_.kf_mode = VPX_KF_AUTO;
  codec_config_.kf_min_dist = 0;
  codec_config_.kf_max_dist = 30000;

  // Do not saturate CPU utilization just for encoding. On a lower-end system
  // with only 1 or 2 cores, use only one thread for encoding. On systems with
  // more cores, allow half of the cores to be used for encoding.
  codec_config_.g_threads =
      std::min(8, (base::SysInfo::NumberOfProcessors() + 1) / 2);

  // Number of frames to consume before producing output.
  codec_config_.g_lag_in_frames = 0;

  DCHECK(!encoder_);
  encoder_.reset(new vpx_codec_ctx_t);
  const vpx_codec_err_t ret = vpx_codec_enc_init(encoder_.get(), interface,
                                                 &codec_config_, kNoFlags);
  DCHECK_EQ(VPX_CODEC_OK, ret);
}

bool VideoTrackRecorder::VpxEncoder::IsInitialized() const {
  DCHECK(encoding_thread_->task_runner()->BelongsToCurrentThread());
  return codec_config_.g_timebase.den != 0;
}

base::TimeDelta VideoTrackRecorder::VpxEncoder::CalculateFrameDuration(
    const scoped_refptr<VideoFrame>& frame) {
  DCHECK(encoding_thread_->task_runner()->BelongsToCurrentThread());

  using base::TimeDelta;
  TimeDelta predicted_frame_duration;
  if (!frame->metadata()->GetTimeDelta(VideoFrameMetadata::FRAME_DURATION,
                                       &predicted_frame_duration) ||
      predicted_frame_duration <= TimeDelta()) {
    // The source of the video frame did not provide the frame duration.  Use
    // the actual amount of time between the current and previous frame as a
    // prediction for the next frame's duration.
    // TODO(mcasas): This duration estimation could lead to artifacts if the
    // cadence of the received stream is compromised (e.g. camera freeze, pause,
    // remote packet loss).  Investigate using GetFrameRate() in this case.
    predicted_frame_duration = frame->timestamp() - last_frame_timestamp_;
  }
  last_frame_timestamp_ = frame->timestamp();
  // Make sure |predicted_frame_duration| is in a safe range of values.
  const TimeDelta kMaxFrameDuration = TimeDelta::FromSecondsD(1.0 / 8);
  const TimeDelta kMinFrameDuration = TimeDelta::FromMilliseconds(1);
  return std::min(kMaxFrameDuration, std::max(predicted_frame_duration,
                                              kMinFrameDuration));
}

VideoTrackRecorder::VideoTrackRecorder(
    bool use_vp9,
    const blink::WebMediaStreamTrack& track,
    const OnEncodedVideoCB& on_encoded_video_callback)
    : track_(track),
      encoder_(new VpxEncoder(use_vp9, on_encoded_video_callback)) {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(!track_.isNull());
  DCHECK(track_.extraData());

  // StartFrameEncode() will be called on Render IO thread.
  AddToVideoTrack(this,
                  base::Bind(&VideoTrackRecorder::VpxEncoder::StartFrameEncode,
                             encoder_),
                  track_);
}

VideoTrackRecorder::~VideoTrackRecorder() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  RemoveFromVideoTrack(this, track_);
  track_.reset();
}

void VideoTrackRecorder::Pause() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(encoder_);
  encoder_->set_paused(true);
}

void VideoTrackRecorder::Resume() {
  DCHECK(main_render_thread_checker_.CalledOnValidThread());
  DCHECK(encoder_);
  encoder_->set_paused(false);
}

void VideoTrackRecorder::OnVideoFrameForTesting(
    const scoped_refptr<media::VideoFrame>& frame,
    base::TimeTicks timestamp) {
  encoder_->StartFrameEncode(frame, timestamp);
}

}  // namespace content
