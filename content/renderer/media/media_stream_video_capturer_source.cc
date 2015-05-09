// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_capturer_source.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "content/public/common/media_stream_request.h"
#include "content/renderer/media/media_stream_constraints_util.h"
#include "content/renderer/media/video_capture_impl_manager.h"
#include "content/renderer/render_thread_impl.h"
#include "media/base/bind_to_current_loop.h"
#include "media/base/limits.h"
#include "media/base/video_frame.h"

namespace content {

namespace {

// Resolutions used if the source doesn't support capability enumeration.
struct {
  int width;
  int height;
} const kVideoResolutions[] = {{1920, 1080},
                               {1280, 720},
                               {960, 720},
                               {640, 480},
                               {640, 360},
                               {320, 240},
                               {320, 180}};

// Frame rates for sources with no support for capability enumeration.
const int kVideoFrameRates[] = {30, 60};

// Hard upper-bound frame rate for tab/desktop capture.
const double kMaxScreenCastFrameRate = 120.0;

// Returns true if the value for width or height is reasonable.
bool DimensionValueIsValid(int x) {
  return x > 0 && x <= media::limits::kMaxDimension;
}

// Returns true if the value for frame rate is reasonable.
bool FrameRateValueIsValid(double frame_rate) {
  return (frame_rate > (1.0 / 60.0)) &&  // Lower-bound: One frame per minute.
      (frame_rate <= media::limits::kMaxFramesPerSecond);
}

// Returns true if the aspect ratio of |a| and |b| are equivalent to two
// significant digits.
bool AreNearlyEquivalentInAspectRatio(const gfx::Size& a, const gfx::Size& b) {
  DCHECK(!a.IsEmpty());
  DCHECK(!b.IsEmpty());
  const int aspect_ratio_a = (100 * a.width()) / a.height();
  const int aspect_ratio_b = (100 * b.width()) / b.height();
  return aspect_ratio_a == aspect_ratio_b;
}

// Interprets the properties in |constraints| to override values in |params| and
// determine the resolution change policy.
void SetScreenCastParamsFromConstraints(
    const blink::WebMediaConstraints& constraints,
    MediaStreamType type,
    media::VideoCaptureParams* params) {
  // The default resolution change policies for tab versus desktop capture are
  // the way they are for legacy reasons.
  if (type == MEDIA_TAB_VIDEO_CAPTURE) {
    params->resolution_change_policy =
        media::RESOLUTION_POLICY_FIXED_RESOLUTION;
  } else if (type == MEDIA_DESKTOP_VIDEO_CAPTURE) {
    params->resolution_change_policy =
        media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT;
  } else {
    NOTREACHED();
  }

  // If the maximum frame resolution was provided in the constraints, use it if
  // either: 1) none has been set yet; or 2) the maximum specificed is smaller
  // than the current setting.
  int width = 0;
  int height = 0;
  gfx::Size desired_max_frame_size;
  if (GetConstraintValueAsInteger(constraints,
                                  MediaStreamVideoSource::kMaxWidth,
                                  &width) &&
      GetConstraintValueAsInteger(constraints,
                                  MediaStreamVideoSource::kMaxHeight,
                                  &height) &&
      DimensionValueIsValid(width) &&
      DimensionValueIsValid(height)) {
    desired_max_frame_size.SetSize(width, height);
    if (params->requested_format.frame_size.IsEmpty() ||
        desired_max_frame_size.width() <
            params->requested_format.frame_size.width() ||
        desired_max_frame_size.height() <
            params->requested_format.frame_size.height()) {
      params->requested_format.frame_size = desired_max_frame_size;
    }
  }

  // Set the default frame resolution if none was provided.
  if (params->requested_format.frame_size.IsEmpty()) {
    params->requested_format.frame_size.SetSize(
        MediaStreamVideoSource::kDefaultWidth,
        MediaStreamVideoSource::kDefaultHeight);
  }

  // If the maximum frame rate was provided, use it if either: 1) none has been
  // set yet; or 2) the maximum specificed is smaller than the current setting.
  double frame_rate = 0.0;
  if (GetConstraintValueAsDouble(constraints,
                                 MediaStreamVideoSource::kMaxFrameRate,
                                 &frame_rate) &&
      FrameRateValueIsValid(frame_rate)) {
    if (params->requested_format.frame_rate <= 0.0f ||
        frame_rate < params->requested_format.frame_rate) {
      params->requested_format.frame_rate = frame_rate;
    }
  }

  // Set the default frame rate if none was provided.
  if (params->requested_format.frame_rate <= 0.0f) {
    params->requested_format.frame_rate =
        MediaStreamVideoSource::kDefaultFrameRate;
  }

  // If the minimum frame resolution was provided, compare it to the maximum
  // frame resolution to determine the intended resolution change policy.
  if (!desired_max_frame_size.IsEmpty() &&
      GetConstraintValueAsInteger(constraints,
                                  MediaStreamVideoSource::kMinWidth,
                                  &width) &&
      GetConstraintValueAsInteger(constraints,
                                  MediaStreamVideoSource::kMinHeight,
                                  &height) &&
      width <= desired_max_frame_size.width() &&
      height <= desired_max_frame_size.height()) {
    if (width == desired_max_frame_size.width() &&
        height == desired_max_frame_size.height()) {
      // Constraints explicitly require a single frame resolution.
      params->resolution_change_policy =
          media::RESOLUTION_POLICY_FIXED_RESOLUTION;
    } else if (DimensionValueIsValid(width) &&
               DimensionValueIsValid(height) &&
               AreNearlyEquivalentInAspectRatio(gfx::Size(width, height),
                                                desired_max_frame_size)) {
      // Constraints only mention a single aspect ratio.
      params->resolution_change_policy =
          media::RESOLUTION_POLICY_FIXED_ASPECT_RATIO;
    } else {
      // Constraints specify a minimum resolution that is smaller than the
      // maximum resolution and has a different aspect ratio (possibly even
      // 0x0). This indicates any frame resolution and aspect ratio is
      // acceptable.
      params->resolution_change_policy =
          media::RESOLUTION_POLICY_ANY_WITHIN_LIMIT;
    }
  }

  DVLOG(1) << "SetScreenCastParamsFromConstraints: "
           << params->requested_format.ToString()
           << " with resolution change policy "
           << params->resolution_change_policy;
}

}  // namespace

VideoCapturerDelegate::VideoCapturerDelegate(
    const StreamDeviceInfo& device_info)
    : session_id_(device_info.session_id),
      is_screen_cast_(device_info.device.type == MEDIA_TAB_VIDEO_CAPTURE ||
                      device_info.device.type == MEDIA_DESKTOP_VIDEO_CAPTURE),
      weak_factory_(this) {
  DVLOG(3) << "VideoCapturerDelegate::ctor";

  // NULL in unit test.
  if (RenderThreadImpl::current()) {
    VideoCaptureImplManager* manager =
        RenderThreadImpl::current()->video_capture_impl_manager();
    if (manager)
      release_device_cb_ = manager->UseDevice(session_id_);
  }
}

VideoCapturerDelegate::~VideoCapturerDelegate() {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(3) << "VideoCapturerDelegate::dtor";
  if (!release_device_cb_.is_null())
    release_device_cb_.Run();
}

void VideoCapturerDelegate::GetCurrentSupportedFormats(
    int max_requested_width,
    int max_requested_height,
    double max_requested_frame_rate,
    const VideoCaptureDeviceFormatsCB& callback) {
  DVLOG(3)
      << "GetCurrentSupportedFormats("
      << " { max_requested_height = " << max_requested_height << "})"
      << " { max_requested_width = " << max_requested_width << "})"
      << " { max_requested_frame_rate = " << max_requested_frame_rate << "})";

  if (is_screen_cast_) {
    const int width = max_requested_width ?
        max_requested_width : MediaStreamVideoSource::kDefaultWidth;
    const int height = max_requested_height ?
        max_requested_height : MediaStreamVideoSource::kDefaultHeight;
    callback.Run(media::VideoCaptureFormats(1, media::VideoCaptureFormat(
        gfx::Size(width, height),
        static_cast<float>(std::min(kMaxScreenCastFrameRate,
                                    max_requested_frame_rate)),
        media::PIXEL_FORMAT_I420)));
    return;
  }

  // NULL in unit test.
  if (!RenderThreadImpl::current())
    return;
  VideoCaptureImplManager* const manager =
      RenderThreadImpl::current()->video_capture_impl_manager();
  if (!manager)
    return;
  DCHECK(source_formats_callback_.is_null());
  source_formats_callback_ = callback;
  manager->GetDeviceFormatsInUse(
      session_id_,
      media::BindToCurrentLoop(
          base::Bind(
              &VideoCapturerDelegate::OnDeviceFormatsInUseReceived,
              weak_factory_.GetWeakPtr())));
}

void VideoCapturerDelegate::StartCapture(
    const media::VideoCaptureParams& params,
    const VideoCaptureDeliverFrameCB& new_frame_callback,
    scoped_refptr<base::SingleThreadTaskRunner> frame_callback_task_runner,
    const RunningCallback& running_callback) {
  DCHECK(params.requested_format.IsValid());
  DCHECK(thread_checker_.CalledOnValidThread());
  running_callback_ = running_callback;

  // NULL in unit test.
  if (!RenderThreadImpl::current())
    return;
  VideoCaptureImplManager* const manager =
      RenderThreadImpl::current()->video_capture_impl_manager();
  if (!manager)
    return;
  if (frame_callback_task_runner !=
      RenderThreadImpl::current()->GetIOMessageLoopProxy()) {
    DCHECK(false) << "Only IO thread supported right now.";
    running_callback.Run(false);
    return;
  }

  stop_capture_cb_ =
      manager->StartCapture(
          session_id_,
    params,
    media::BindToCurrentLoop(base::Bind(
              &VideoCapturerDelegate::OnStateUpdate,
        weak_factory_.GetWeakPtr())),
          new_frame_callback);
}

void VideoCapturerDelegate::StopCapture() {
  // Immediately make sure we don't provide more frames.
  DVLOG(3) << "VideoCapturerDelegate::StopCapture()";
  DCHECK(thread_checker_.CalledOnValidThread());
  if (!stop_capture_cb_.is_null()) {
    base::ResetAndReturn(&stop_capture_cb_).Run();
  }
  running_callback_.Reset();
  source_formats_callback_.Reset();
}

void VideoCapturerDelegate::OnStateUpdate(
    VideoCaptureState state) {
  DCHECK(thread_checker_.CalledOnValidThread());
  DVLOG(3) << "OnStateUpdate state = " << state;
  if (state == VIDEO_CAPTURE_STATE_STARTED && !running_callback_.is_null()) {
    running_callback_.Run(true);
    return;
  }
  if (state > VIDEO_CAPTURE_STATE_STARTED && !running_callback_.is_null()) {
    base::ResetAndReturn(&running_callback_).Run(false);
  }
}

void VideoCapturerDelegate::OnDeviceFormatsInUseReceived(
    const media::VideoCaptureFormats& formats_in_use) {
  DVLOG(3) << "OnDeviceFormatsInUseReceived: " << formats_in_use.size();
  DCHECK(thread_checker_.CalledOnValidThread());
  // StopCapture() might have destroyed |source_formats_callback_| before
  // arriving here.
  if (source_formats_callback_.is_null())
    return;
  // If there are no formats in use, try to retrieve the whole list of
  // supported form.
  if (!formats_in_use.empty()) {
    source_formats_callback_.Run(formats_in_use);
    source_formats_callback_.Reset();
    return;
  }

  // NULL in unit test.
  if (!RenderThreadImpl::current())
    return;
  VideoCaptureImplManager* const manager =
      RenderThreadImpl::current()->video_capture_impl_manager();
  if (!manager)
    return;

  manager->GetDeviceSupportedFormats(
      session_id_,
      media::BindToCurrentLoop(
          base::Bind(
              &VideoCapturerDelegate::OnDeviceSupportedFormatsEnumerated,
              weak_factory_.GetWeakPtr())));
}

void VideoCapturerDelegate::OnDeviceSupportedFormatsEnumerated(
    const media::VideoCaptureFormats& formats) {
  DVLOG(3) << "OnDeviceSupportedFormatsEnumerated: " << formats.size()
           << " received";
  DCHECK(thread_checker_.CalledOnValidThread());
  // StopCapture() might have destroyed |source_formats_callback_| before
  // arriving here.
  if (source_formats_callback_.is_null())
    return;
  if (formats.size()) {
    base::ResetAndReturn(&source_formats_callback_).Run(formats);
  } else {
    // The capture device doesn't seem to support capability enumeration,
    // compose a fallback list of capabilities.
    media::VideoCaptureFormats default_formats;
    for (const auto& resolution : kVideoResolutions) {
      for (const auto frame_rate : kVideoFrameRates) {
        default_formats.push_back(media::VideoCaptureFormat(
            gfx::Size(resolution.width, resolution.height),
            frame_rate,
            media::PIXEL_FORMAT_I420));
      }
    }
    base::ResetAndReturn(&source_formats_callback_).Run(default_formats);
  }
}

MediaStreamVideoCapturerSource::MediaStreamVideoCapturerSource(
    const SourceStoppedCallback& stop_callback,
    scoped_ptr<media::VideoCapturerSource> delegate)
    : delegate_(delegate.Pass()) {
  SetStopCallback(stop_callback);
}

MediaStreamVideoCapturerSource::~MediaStreamVideoCapturerSource() {
}

void MediaStreamVideoCapturerSource::SetDeviceInfo(
    const StreamDeviceInfo& device_info) {
  MediaStreamVideoSource::SetDeviceInfo(device_info);
}

void MediaStreamVideoCapturerSource::GetCurrentSupportedFormats(
    int max_requested_width,
    int max_requested_height,
    double max_requested_frame_rate,
    const VideoCaptureDeviceFormatsCB& callback) {
  delegate_->GetCurrentSupportedFormats(
      max_requested_width,
      max_requested_height,
      max_requested_frame_rate,
      callback);
}

void MediaStreamVideoCapturerSource::StartSourceImpl(
    const media::VideoCaptureFormat& format,
    const blink::WebMediaConstraints& constraints,
    const VideoCaptureDeliverFrameCB& frame_callback) {
  media::VideoCaptureParams new_params;
  new_params.requested_format = format;
  if (device_info().device.type == MEDIA_TAB_VIDEO_CAPTURE ||
      device_info().device.type == MEDIA_DESKTOP_VIDEO_CAPTURE) {
    SetScreenCastParamsFromConstraints(
        constraints, device_info().device.type, &new_params);
  }
  delegate_->StartCapture(
      new_params,
      frame_callback,
      RenderThreadImpl::current() ?
      RenderThreadImpl::current()->GetIOMessageLoopProxy() :
      nullptr,
      base::Bind(&MediaStreamVideoCapturerSource::OnStarted,
                 base::Unretained(this)));
}

void MediaStreamVideoCapturerSource::StopSourceImpl() {
  delegate_->StopCapture();
}

void MediaStreamVideoCapturerSource::OnStarted(bool result) {
  OnStartDone(result ? MEDIA_DEVICE_OK : MEDIA_DEVICE_TRACK_START_FAILURE);
}

}  // namespace content
