// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "media/capture/content/android/screen_capture_machine_android.h"

#include "base/android/context_utils.h"
#include "base/android/jni_android.h"
#include "base/android/scoped_java_ref.h"
#include "jni/ScreenCapture_jni.h"
#include "media/base/video_capture_types.h"
#include "media/base/yuv_convert.h"
#include "media/capture/content/video_capture_oracle.h"
#include "third_party/libyuv/include/libyuv.h"

using base::android::AttachCurrentThread;

namespace media {

// static
bool ScreenCaptureMachineAndroid::RegisterScreenCaptureMachine(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

ScreenCaptureMachineAndroid::ScreenCaptureMachineAndroid() {}

ScreenCaptureMachineAndroid::~ScreenCaptureMachineAndroid() {}

// static
ScopedJavaLocalRef<jobject>
ScreenCaptureMachineAndroid::createScreenCaptureMachineAndroid(
    jlong nativeScreenCaptureMachineAndroid) {
  return (Java_ScreenCapture_createScreenCaptureMachine(
      AttachCurrentThread(), base::android::GetApplicationContext(),
      nativeScreenCaptureMachineAndroid));
}

void ScreenCaptureMachineAndroid::OnRGBAFrameAvailable(JNIEnv* env,
                                                       jobject obj,
                                                       jobject buf,
                                                       jint row_stride,
                                                       jint left,
                                                       jint top,
                                                       jint width,
                                                       jint height,
                                                       jlong timestamp) {
  const VideoCaptureOracle::Event event = VideoCaptureOracle::kCompositorUpdate;
  const uint64_t absolute_micro =
      timestamp / base::Time::kNanosecondsPerMicrosecond;
  const base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(absolute_micro);
  scoped_refptr<VideoFrame> frame;
  ThreadSafeCaptureOracle::CaptureFrameCallback capture_frame_cb;

  if (!oracle_proxy_->ObserveEventAndDecideCapture(
          event, gfx::Rect(), start_time, &frame, &capture_frame_cb)) {
    return;
  }

  DCHECK(frame->format() == PIXEL_FORMAT_I420 ||
         frame->format() == PIXEL_FORMAT_YV12);

  scoped_refptr<VideoFrame> unscaled_frame = frame;
  if (frame->visible_rect().width() != width ||
      frame->visible_rect().height() != height) {
    unscaled_frame = VideoFrame::CreateFrame(
        PIXEL_FORMAT_I420, gfx::Size(width, height), gfx::Rect(width, height),
        gfx::Size(width, height), base::TimeDelta());
  }

  uint8_t* const src =
      reinterpret_cast<uint8_t*>(env->GetDirectBufferAddress(buf));
  CHECK(src);

  const int offset = top * row_stride + left * 4;
  // ABGR little endian (rgba in memory) to I420.
  libyuv::ABGRToI420(src + offset, row_stride,
                     unscaled_frame->visible_data(VideoFrame::kYPlane),
                     unscaled_frame->stride(VideoFrame::kYPlane),
                     unscaled_frame->visible_data(VideoFrame::kUPlane),
                     unscaled_frame->stride(VideoFrame::kUPlane),
                     unscaled_frame->visible_data(VideoFrame::kVPlane),
                     unscaled_frame->stride(VideoFrame::kVPlane),
                     unscaled_frame->visible_rect().width(),
                     unscaled_frame->visible_rect().height());

  if (unscaled_frame != frame) {
    libyuv::I420Scale(unscaled_frame->visible_data(VideoFrame::kYPlane),
                      unscaled_frame->stride(VideoFrame::kYPlane),
                      unscaled_frame->visible_data(VideoFrame::kUPlane),
                      unscaled_frame->stride(VideoFrame::kUPlane),
                      unscaled_frame->visible_data(VideoFrame::kVPlane),
                      unscaled_frame->stride(VideoFrame::kVPlane),
                      unscaled_frame->visible_rect().width(),
                      unscaled_frame->visible_rect().height(),
                      frame->visible_data(VideoFrame::kYPlane),
                      frame->stride(VideoFrame::kYPlane),
                      frame->visible_data(VideoFrame::kUPlane),
                      frame->stride(VideoFrame::kUPlane),
                      frame->visible_data(VideoFrame::kVPlane),
                      frame->stride(VideoFrame::kVPlane),
                      frame->visible_rect().width(),
                      frame->visible_rect().height(), libyuv::kFilterBilinear);
  }

  capture_frame_cb.Run(frame, start_time, true);

  lastFrame_ = frame;
}

void ScreenCaptureMachineAndroid::OnI420FrameAvailable(JNIEnv* env,
                                                       jobject obj,
                                                       jobject y_buffer,
                                                       jint y_stride,
                                                       jobject u_buffer,
                                                       jobject v_buffer,
                                                       jint uv_row_stride,
                                                       jint uv_pixel_stride,
                                                       jint left,
                                                       jint top,
                                                       jint width,
                                                       jint height,
                                                       jlong timestamp) {
  const VideoCaptureOracle::Event event = VideoCaptureOracle::kCompositorUpdate;
  const uint64_t absolute_micro =
      timestamp / base::Time::kNanosecondsPerMicrosecond;
  const base::TimeTicks start_time =
      base::TimeTicks() + base::TimeDelta::FromMicroseconds(absolute_micro);
  scoped_refptr<VideoFrame> frame;
  ThreadSafeCaptureOracle::CaptureFrameCallback capture_frame_cb;

  if (!oracle_proxy_->ObserveEventAndDecideCapture(
          event, gfx::Rect(), start_time, &frame, &capture_frame_cb)) {
    return;
  }

  DCHECK(frame->format() == PIXEL_FORMAT_I420 ||
         frame->format() == PIXEL_FORMAT_YV12);

  uint8_t* const y_src =
      reinterpret_cast<uint8_t*>(env->GetDirectBufferAddress(y_buffer));
  CHECK(y_src);
  uint8_t* u_src =
      reinterpret_cast<uint8_t*>(env->GetDirectBufferAddress(u_buffer));
  CHECK(u_src);
  uint8_t* v_src =
      reinterpret_cast<uint8_t*>(env->GetDirectBufferAddress(v_buffer));
  CHECK(v_src);

  // De-interleave the U and V planes into temporary buffers, if needed.
  int uv_stride = uv_row_stride;
  std::unique_ptr<uint8_t[]> u_tmp, v_tmp;
  if (uv_pixel_stride != 1) {
    // U and V planes are actually interleaved, unpack them here.
    // TODO(braveyao): According to
    // https://developer.android.com/reference/android/graphics/ImageFormat.html#YUV_420_888,
    // how U and V planes are interlaced is not guaranteed, so there is no an
    // existing libyuv function suitable for such a job. Filed a request at
    // https://bugs.chromium.org/p/libyuv/issues/detail?id=604. Switch to new
    // function when it's available.
    const int uv_plane_len = (int)env->GetDirectBufferCapacity(u_buffer);
    u_tmp.reset(new uint8_t[(uv_plane_len + 1) / uv_pixel_stride]);
    v_tmp.reset(new uint8_t[(uv_plane_len + 1) / uv_pixel_stride]);
    for (int index = 0; index * uv_pixel_stride <= uv_plane_len; index++) {
      u_tmp[index] = u_src[index * uv_pixel_stride];
      v_tmp[index] = v_src[index * uv_pixel_stride];
    }
    u_src = u_tmp.get();
    v_src = v_tmp.get();
    uv_stride /= uv_pixel_stride;
  }

  const int y_offset = top * y_stride + left;
  const int uv_offset = (top / 2) * uv_stride + left / 2;
  // Note: If source width/height are same as the frame's width/height, the
  // following will, internally, just perform a copy without scaling.
  libyuv::I420Scale(y_src + y_offset, y_stride, u_src + uv_offset, uv_stride,
                    v_src + uv_offset, uv_stride, width, height,
                    frame->visible_data(VideoFrame::kYPlane),
                    frame->stride(VideoFrame::kYPlane),
                    frame->visible_data(VideoFrame::kUPlane),
                    frame->stride(VideoFrame::kUPlane),
                    frame->visible_data(VideoFrame::kVPlane),
                    frame->stride(VideoFrame::kVPlane),
                    frame->visible_rect().width(),
                    frame->visible_rect().height(), libyuv::kFilterBilinear);

  capture_frame_cb.Run(frame, start_time, true);

  lastFrame_ = frame;
}

void ScreenCaptureMachineAndroid::OnActivityResult(JNIEnv* env,
                                                   jobject obj,
                                                   jboolean result) {
  if (!result) {
    oracle_proxy_->ReportError(FROM_HERE, "The user denied screen capture");
    return;
  }

  Java_ScreenCapture_startCapture(env, obj);
}

void ScreenCaptureMachineAndroid::Start(
    const scoped_refptr<ThreadSafeCaptureOracle>& oracle_proxy,
    const VideoCaptureParams& params,
    const base::Callback<void(bool)> callback) {
  DCHECK(oracle_proxy.get());
  oracle_proxy_ = oracle_proxy;

  j_capture_.Reset(
      createScreenCaptureMachineAndroid(reinterpret_cast<intptr_t>(this)));

  if (j_capture_.obj() == nullptr) {
    DLOG(ERROR) << "Failed to createScreenCaptureAndroid";
    callback.Run(false);
    return;
  }

  DCHECK(params.requested_format.frame_size.GetArea());
  DCHECK(!(params.requested_format.frame_size.width() % 2));
  DCHECK(!(params.requested_format.frame_size.height() % 2));

  const jboolean ret = Java_ScreenCapture_startPrompt(
      AttachCurrentThread(), j_capture_.obj(),
      params.requested_format.frame_size.width(),
      params.requested_format.frame_size.height());

  callback.Run(ret);
}

void ScreenCaptureMachineAndroid::Stop(const base::Closure& callback) {
  Java_ScreenCapture_stopCapture(AttachCurrentThread(), j_capture_.obj());

  callback.Run();
}

// ScreenCapture on Android works in a passive way and there are no captured
// frames when there is no update to the screen. When the oracle asks for a
// capture refresh, the cached captured frame is redelivered.
void ScreenCaptureMachineAndroid::MaybeCaptureForRefresh() {
  if (lastFrame_.get() == nullptr)
    return;

  const VideoCaptureOracle::Event event =
      VideoCaptureOracle::kActiveRefreshRequest;
  const base::TimeTicks start_time = base::TimeTicks::Now();
  scoped_refptr<VideoFrame> frame;
  ThreadSafeCaptureOracle::CaptureFrameCallback capture_frame_cb;

  if (!oracle_proxy_->ObserveEventAndDecideCapture(
          event, gfx::Rect(), start_time, &frame, &capture_frame_cb)) {
    return;
  }

  DCHECK(frame->format() == PIXEL_FORMAT_I420 ||
         frame->format() == PIXEL_FORMAT_YV12);

  libyuv::I420Scale(
      lastFrame_->visible_data(VideoFrame::kYPlane),
      lastFrame_->stride(VideoFrame::kYPlane),
      lastFrame_->visible_data(VideoFrame::kUPlane),
      lastFrame_->stride(VideoFrame::kUPlane),
      lastFrame_->visible_data(VideoFrame::kVPlane),
      lastFrame_->stride(VideoFrame::kVPlane),
      lastFrame_->visible_rect().width(), lastFrame_->visible_rect().height(),
      frame->visible_data(VideoFrame::kYPlane),
      frame->stride(VideoFrame::kYPlane),
      frame->visible_data(VideoFrame::kUPlane),
      frame->stride(VideoFrame::kUPlane),
      frame->visible_data(VideoFrame::kVPlane),
      frame->stride(VideoFrame::kVPlane), frame->visible_rect().width(),
      frame->visible_rect().height(), libyuv::kFilterBilinear);

  capture_frame_cb.Run(frame, start_time, true);
}

}  // namespace media
