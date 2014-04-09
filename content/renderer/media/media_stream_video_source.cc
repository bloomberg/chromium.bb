// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_source.h"

#include <algorithm>
#include <limits>
#include <string>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
#include "content/renderer/media/media_stream_video_track.h"
#include "content/renderer/media/webrtc/webrtc_video_capturer_adapter.h"

namespace content {

// Constraint keys. Specified by draft-alvestrand-constraints-resolution-00b
const char MediaStreamVideoSource::kMinAspectRatio[] = "minAspectRatio";
const char MediaStreamVideoSource::kMaxAspectRatio[] = "maxAspectRatio";
const char MediaStreamVideoSource::kMaxWidth[] = "maxWidth";
const char MediaStreamVideoSource::kMinWidth[] = "minWidth";
const char MediaStreamVideoSource::kMaxHeight[] = "maxHeight";
const char MediaStreamVideoSource::kMinHeight[] = "minHeight";
const char MediaStreamVideoSource::kMaxFrameRate[] = "maxFrameRate";
const char MediaStreamVideoSource::kMinFrameRate[] = "minFrameRate";

const char* kSupportedConstraints[] = {
    MediaStreamVideoSource::kMaxAspectRatio,
    MediaStreamVideoSource::kMinAspectRatio,
    MediaStreamVideoSource::kMaxWidth,
    MediaStreamVideoSource::kMinWidth,
    MediaStreamVideoSource::kMaxHeight,
    MediaStreamVideoSource::kMinHeight,
    MediaStreamVideoSource::kMaxFrameRate,
    MediaStreamVideoSource::kMinFrameRate,
};

const int MediaStreamVideoSource::kDefaultWidth = 640;
const int MediaStreamVideoSource::kDefaultHeight = 480;
const int MediaStreamVideoSource::kDefaultFrameRate = 30;

namespace {
// Constraints keys for http://dev.w3.org/2011/webrtc/editor/getusermedia.html
const char kSourceId[] = "sourceId";

// Google-specific key prefix. Constraints with this prefix are ignored if they
// are unknown.
const char kGooglePrefix[] = "goog";

// MediaStreamVideoSource supports cropping of video frames but only up to
// kMaxCropFactor. Ie - if a constraint is set to maxHeight 360, an original
// input frame height of max 360 * kMaxCropFactor pixels is accepted.
const int kMaxCropFactor = 2;

// Returns true if |constraint| is fulfilled. |format| can be changed
// changed by a constraint. Ie - the frame rate can be changed by setting
// maxFrameRate.
bool UpdateFormatForConstraint(
    const blink::WebMediaConstraint& constraint,
    bool mandatory,
    media::VideoCaptureFormat* format) {
  DCHECK(format != NULL);

  if (!format->IsValid())
    return false;

  std::string constraint_name = constraint.m_name.utf8();
  std::string constraint_value = constraint.m_value.utf8();

  if (constraint_name.find(kGooglePrefix) == 0) {
    // These are actually options, not constraints, so they can be satisfied
    // regardless of the format.
    return true;
  }

  if (constraint_name == kSourceId) {
    // This is a constraint that doesn't affect the format.
    return true;
  }

  // Ignore Chrome specific Tab capture constraints.
  if (constraint_name == kMediaStreamSource ||
      constraint_name == kMediaStreamSourceId)
    return true;

  if (constraint_name == MediaStreamVideoSource::kMinAspectRatio ||
      constraint_name == MediaStreamVideoSource::kMaxAspectRatio) {
    double double_value = 0;
    base::StringToDouble(constraint_value, &double_value);

    // The aspect ratio in |constraint.m_value| has been converted to a string
    // and back to a double, so it may have a rounding error.
    // E.g if the value 1/3 is converted to a string, the string will not have
    // infinite length.
    // We add a margin of 0.0005 which is high enough to detect the same aspect
    // ratio but small enough to avoid matching wrong aspect ratios.
    const double kRoundingTruncation = 0.0005;
    double ratio = static_cast<double>(format->frame_size.width()) /
        format->frame_size.height();
    if (constraint_name == MediaStreamVideoSource::kMinAspectRatio)
      return (double_value <= ratio + kRoundingTruncation);
    // Subtract 0.0005 to avoid rounding problems. Same as above.
    return (double_value >= ratio - kRoundingTruncation);
  }

  int value;
  if (!base::StringToInt(constraint_value, &value)) {
    DLOG(WARNING) << "Can't parse MediaStream constraint. Name:"
                  <<  constraint_name << " Value:" << constraint_value;
    return false;
  }
  if (constraint_name == MediaStreamVideoSource::kMinWidth) {
    return (value <= format->frame_size.width());
  } else if (constraint_name == MediaStreamVideoSource::kMaxWidth) {
    return (value * kMaxCropFactor >= format->frame_size.width());
  } else if (constraint_name == MediaStreamVideoSource::kMinHeight) {
    return (value <= format->frame_size.height());
  } else if (constraint_name == MediaStreamVideoSource::kMaxHeight) {
     return (value * kMaxCropFactor >= format->frame_size.height());
  } else if (constraint_name == MediaStreamVideoSource::kMinFrameRate) {
    return (value <= format->frame_rate);
  } else if (constraint_name == MediaStreamVideoSource::kMaxFrameRate) {
    if (value == 0) {
      // The frame rate is set by constraint.
      // Don't allow 0 as frame rate if it is a mandatory constraint.
      // Set the frame rate to 1 if it is not mandatory.
      if (mandatory) {
        return false;
      } else {
        value = 1;
      }
    }
    format->frame_rate =
        (format->frame_rate > value) ? value : format->frame_rate;
    return true;
  } else {
    LOG(WARNING) << "Found unknown MediaStream constraint. Name:"
                 <<  constraint_name << " Value:" << constraint_value;
    return false;
  }
}

// Removes media::VideoCaptureFormats from |formats| that don't meet
// |constraint|.
void FilterFormatsByConstraint(
    const blink::WebMediaConstraint& constraint,
    bool mandatory,
    media::VideoCaptureFormats* formats) {
  DVLOG(3) << "FilterFormatsByConstraint("
           << "{ constraint.m_name = " << constraint.m_name.utf8()
           << "  constraint.m_value = " << constraint.m_value.utf8()
           << "  mandatory =  " << mandatory << "})";
  media::VideoCaptureFormats::iterator format_it = formats->begin();
  while (format_it != formats->end()) {
    // Modify the format_it to fulfill the constraint if possible.
    // Delete it otherwise.
    if (!UpdateFormatForConstraint(constraint, mandatory, &(*format_it))) {
      format_it = formats->erase(format_it);
    } else {
      ++format_it;
    }
  }
}

// Returns the media::VideoCaptureFormats that matches |constraints|.
media::VideoCaptureFormats FilterFormats(
    const blink::WebMediaConstraints& constraints,
    const media::VideoCaptureFormats& supported_formats) {
  if (constraints.isNull()) {
    return supported_formats;
  }

  blink::WebVector<blink::WebMediaConstraint> mandatory;
  blink::WebVector<blink::WebMediaConstraint> optional;
  constraints.getMandatoryConstraints(mandatory);
  constraints.getOptionalConstraints(optional);

  media::VideoCaptureFormats candidates = supported_formats;

  for (size_t i = 0; i < mandatory.size(); ++i)
    FilterFormatsByConstraint(mandatory[i], true, &candidates);

  if (candidates.empty())
    return candidates;

  // Ok - all mandatory checked and we still have candidates.
  // Let's try filtering using the optional constraints. The optional
  // constraints must be filtered in the order they occur in |optional|.
  // But if a constraint produce zero candidates, the constraint is ignored and
  // the next constraint is tested.
  // http://dev.w3.org/2011/webrtc/editor/getusermedia.html#idl-def-Constraints
  for (size_t i = 0; i < optional.size(); ++i) {
    media::VideoCaptureFormats current_candidates = candidates;
    FilterFormatsByConstraint(optional[i], false, &current_candidates);
    if (!current_candidates.empty()) {
      candidates = current_candidates;
    }
  }

  // We have done as good as we can to filter the supported resolutions.
  return candidates;
}

bool GetConstraintValue(const blink::WebMediaConstraints& constraints,
                        bool mandatory, const blink::WebString& name,
                        int* value) {
  blink::WebString value_str;
  bool ret = mandatory ?
      constraints.getMandatoryConstraintValue(name, value_str) :
      constraints.getOptionalConstraintValue(name, value_str);
  if (ret)
    base::StringToInt(value_str.utf8(), value);
  return ret;
}

// Retrieve the desired max width and height from |constraints|.
void GetDesiredMaxWidthAndHeight(const blink::WebMediaConstraints& constraints,
                                 int* desired_width, int* desired_height) {
  bool mandatory = GetConstraintValue(constraints, true,
                                      MediaStreamVideoSource::kMaxWidth,
                                      desired_width);
  mandatory |= GetConstraintValue(constraints, true,
                                  MediaStreamVideoSource::kMaxHeight,
                                  desired_height);
  if (mandatory)
    return;

  GetConstraintValue(constraints, false, MediaStreamVideoSource::kMaxWidth,
                     desired_width);
  GetConstraintValue(constraints, false, MediaStreamVideoSource::kMaxHeight,
                     desired_height);
}

const media::VideoCaptureFormat& GetBestFormatBasedOnArea(
    const media::VideoCaptureFormats& formats,
    int area) {
  media::VideoCaptureFormats::const_iterator it = formats.begin();
  media::VideoCaptureFormats::const_iterator best_it = formats.begin();
  int best_diff = std::numeric_limits<int>::max();
  for (; it != formats.end(); ++it) {
    int diff = abs(area - it->frame_size.width() * it->frame_size.height());
    if (diff < best_diff) {
      best_diff = diff;
      best_it = it;
    }
  }
  return *best_it;
}

// Find the format that best matches the default video size.
// This algorithm is chosen since a resolution must be picked even if no
// constraints are provided. We don't just select the maximum supported
// resolution since higher resolutions cost more in terms of complexity and
// many cameras have lower frame rate and have more noise in the image at
// their maximum supported resolution.
void GetBestCaptureFormat(
    const media::VideoCaptureFormats& formats,
    const blink::WebMediaConstraints& constraints,
    media::VideoCaptureFormat* capture_format,
    gfx::Size* frame_output_size) {
  DCHECK(!formats.empty());
  DCHECK(frame_output_size);

  int max_width = std::numeric_limits<int>::max();
  int max_height = std::numeric_limits<int>::max();;
  GetDesiredMaxWidthAndHeight(constraints, &max_width, &max_height);

  *capture_format = GetBestFormatBasedOnArea(
      formats,
      std::min(max_width, MediaStreamVideoSource::kDefaultWidth) *
      std::min(max_height, MediaStreamVideoSource::kDefaultHeight));

  *frame_output_size = capture_format->frame_size;
  if (max_width < frame_output_size->width())
    frame_output_size->set_width(max_width);
  if (max_height < frame_output_size->height())
    frame_output_size->set_height(max_height);
}

// Empty method used for keeping a reference to the original media::VideoFrame
// in MediaStreamVideoSource::DeliverVideoFrame if cropping is needed.
// The reference to |frame| is kept in the closure that calls this method.
void ReleaseOriginalFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
}

}  // anonymous namespace

// static
MediaStreamVideoSource* MediaStreamVideoSource::GetVideoSource(
    const blink::WebMediaStreamSource& source) {
  return static_cast<MediaStreamVideoSource*>(source.extraData());
}

//static
bool MediaStreamVideoSource::IsConstraintSupported(const std::string& name) {
  for (size_t i = 0; i < arraysize(kSupportedConstraints); ++i) {
    if (kSupportedConstraints[i] == name)
      return true;
  }
  return false;
}

MediaStreamVideoSource::MediaStreamVideoSource(
    MediaStreamDependencyFactory* factory)
    : state_(NEW),
      factory_(factory),
      capture_adapter_(NULL) {
  DCHECK(factory_);
}

MediaStreamVideoSource::~MediaStreamVideoSource() {
}

void MediaStreamVideoSource::AddTrack(
    MediaStreamVideoTrack* track,
    const blink::WebMediaConstraints& constraints,
    const ConstraintsCallback& callback) {
  DCHECK(CalledOnValidThread());
  DCHECK(std::find(tracks_.begin(), tracks_.end(),
                   track) == tracks_.end());
  tracks_.push_back(track);

  requested_constraints_.push_back(
      RequestedConstraints(constraints, callback));

  switch (state_) {
    case NEW: {
      // Tab capture and Screen capture needs the maximum requested height
      // and width to decide on the resolution.
      int max_requested_width = 0;
      GetConstraintValue(constraints, true, kMaxWidth, &max_requested_width);

      int max_requested_height = 0;
      GetConstraintValue(constraints, true, kMaxHeight, &max_requested_height);

      state_ = RETRIEVING_CAPABILITIES;
      GetCurrentSupportedFormats(max_requested_width,
                                 max_requested_height);

      break;
    }
    case STARTING:
    case RETRIEVING_CAPABILITIES: {
      // The |callback| will be triggered once the source has started or
      // the capabilities have been retrieved.
      break;
    }
    case ENDED:
    case STARTED: {
      // Currently, reconfiguring the source is not supported.
      FinalizeAddTrack();
    }
  }
}

void MediaStreamVideoSource::RemoveTrack(MediaStreamVideoTrack* video_track) {
  std::vector<MediaStreamVideoTrack*>::iterator it =
      std::find(tracks_.begin(), tracks_.end(), video_track);
  DCHECK(it != tracks_.end());
  tracks_.erase(it);
}

void MediaStreamVideoSource::InitAdapter() {
  if (adapter_)
    return;
  // Create the webrtc::MediaStreamVideoSourceInterface adapter.
  // It needs the constraints so that constraints used by a PeerConnection
  // will be available such as constraints for CPU adaptation and a tab
  // capture.
  bool is_screencast =
      device_info().device.type == MEDIA_TAB_VIDEO_CAPTURE ||
      device_info().device.type == MEDIA_DESKTOP_VIDEO_CAPTURE;
  capture_adapter_ = factory_->CreateVideoCapturer(is_screencast);
  adapter_ = factory_->CreateVideoSource(capture_adapter_,
                                         current_constraints_);
}

webrtc::VideoSourceInterface* MediaStreamVideoSource::GetAdapter() {
  if (!adapter_) {
    InitAdapter();
  }
  return adapter_;
}

void MediaStreamVideoSource::DoStopSource() {
  DVLOG(3) << "DoStopSource()";
  StopSourceImpl();
  state_ = ENDED;
  SetReadyState(blink::WebMediaStreamSource::ReadyStateEnded);
}

void MediaStreamVideoSource::DeliverVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  scoped_refptr<media::VideoFrame> video_frame(frame);

  if (frame->visible_rect().size() != frame_output_size_) {
    // If |frame| is not the size that is expected, we need to crop it by
    // providing a new |visible_rect|. The new visible rect must be within the
    // original |visible_rect|.
    const int visible_width = std::min(frame_output_size_.width(),
                                       frame->visible_rect().width());

    const int visible_height = std::min(frame_output_size_.height(),
                                        frame->visible_rect().height());

    // Find a new horizontal offset within |frame|.
    const int horiz_crop = frame->visible_rect().x() +
        ((frame->visible_rect().width() - visible_width) / 2);
    // Find a new vertical offset within |frame|.
    const int vert_crop = frame->visible_rect().y() +
        ((frame->visible_rect().height() - visible_height) / 2);

    gfx::Rect rect(horiz_crop, vert_crop, visible_width, visible_height);
    video_frame = media::VideoFrame::WrapVideoFrame(
        frame, rect, rect.size(), base::Bind(&ReleaseOriginalFrame, frame));
  }

  if ((frame->format() == media::VideoFrame::I420 ||
       frame->format() == media::VideoFrame::YV12) &&
      capture_adapter_) {
    capture_adapter_->OnFrameCaptured(video_frame);
  }

  for (std::vector<MediaStreamVideoTrack*>::iterator it = tracks_.begin();
       it != tracks_.end(); ++it) {
    (*it)->OnVideoFrame(video_frame);
  }
}

void MediaStreamVideoSource::OnSupportedFormats(
    const media::VideoCaptureFormats& formats) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(RETRIEVING_CAPABILITIES, state_);

  supported_formats_ = formats;
  if (!FindBestFormatWithConstraints(supported_formats_,
                                     &current_format_,
                                     &frame_output_size_,
                                     &current_constraints_)) {
    FinalizeAddTrack();
    SetReadyState(blink::WebMediaStreamSource::ReadyStateEnded);
    return;
  }

  state_ = STARTING;
  DVLOG(3) << "Starting the capturer with"
           << " width = " << current_format_.frame_size.width()
           << " height = " << current_format_.frame_size.height()
           << " frame rate = " << current_format_.frame_rate;

  media::VideoCaptureParams params;
  params.requested_format = current_format_;
  StartSourceImpl(params);
}

bool MediaStreamVideoSource::FindBestFormatWithConstraints(
    const media::VideoCaptureFormats& formats,
    media::VideoCaptureFormat* best_format,
    gfx::Size* frame_output_size,
    blink::WebMediaConstraints* resulting_constraints) {
  // Find the first constraints that we can fulfill.
  for (std::vector<RequestedConstraints>::iterator request_it =
           requested_constraints_.begin();
       request_it != requested_constraints_.end(); ++request_it) {
    const blink::WebMediaConstraints& requested_constraints =
        request_it->constraints;

    media::VideoCaptureFormats filtered_formats =
        FilterFormats(requested_constraints, formats);
    if (filtered_formats.size() > 0) {
      // A request with constraints that can be fulfilled.
      GetBestCaptureFormat(filtered_formats,
                           requested_constraints,
                           best_format,
                           frame_output_size);
      *resulting_constraints= requested_constraints;
      return true;
    }
  }
  return false;
}

void MediaStreamVideoSource::OnStartDone(bool success) {
  DCHECK(CalledOnValidThread());
  DVLOG(3) << "OnStartDone({success =" << success << "})";
  if (success) {
    DCHECK_EQ(STARTING, state_);
    state_ = STARTED;
    SetReadyState(blink::WebMediaStreamSource::ReadyStateLive);
  } else {
    state_ = ENDED;
    SetReadyState(blink::WebMediaStreamSource::ReadyStateEnded);
    StopSourceImpl();
  }

  FinalizeAddTrack();
}

void MediaStreamVideoSource::FinalizeAddTrack() {
  media::VideoCaptureFormats formats;
  formats.push_back(current_format_);

  std::vector<RequestedConstraints> callbacks;
  callbacks.swap(requested_constraints_);
  for (std::vector<RequestedConstraints>::iterator it = callbacks.begin();
       it != callbacks.end(); ++it) {

    bool success = state_ == STARTED &&
        !FilterFormats(it->constraints, formats).empty();
    DVLOG(3) << "FinalizeAddTrack() success " << success;
    if (!it->callback.is_null())
      it->callback.Run(this, success);
  }
}

void MediaStreamVideoSource::SetReadyState(
    blink::WebMediaStreamSource::ReadyState state) {
  if (!owner().isNull()) {
    owner().setReadyState(state);
  }
  for (std::vector<MediaStreamVideoTrack*>::iterator it = tracks_.begin();
       it != tracks_.end(); ++it) {
    (*it)->OnReadyStateChanged(state);
  }
}

MediaStreamVideoSource::RequestedConstraints::RequestedConstraints(
    const blink::WebMediaConstraints& constraints,
    const ConstraintsCallback& callback)
    : constraints(constraints), callback(callback) {
}

MediaStreamVideoSource::RequestedConstraints::~RequestedConstraints() {
}

}  // namespace content
