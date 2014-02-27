// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/media_stream_video_source.h"

#include <limits>
#include <string>

#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "content/renderer/media/media_stream_dependency_factory.h"
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

const int MediaStreamVideoSource::kDefaultWidth = 640;
const int MediaStreamVideoSource::kDefaultHeight = 480;
const int MediaStreamVideoSource::kDefaultFrameRate = 30;

namespace {
// Constraints keys for http://dev.w3.org/2011/webrtc/editor/getusermedia.html
const char kSourceId[] = "sourceId";

// Google-specific key prefix. Constraints with this prefix are ignored if they
// are unknown.
const char kGooglePrefix[] = "goog";

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
    return (value >= format->frame_size.width());
  } else if (constraint_name == MediaStreamVideoSource::kMinHeight) {
    return (value <= format->frame_size.height());
  } else if (constraint_name == MediaStreamVideoSource::kMaxHeight) {
    return (value >= format->frame_size.height());
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

// Find the format that best matches the default video size.
// This algorithm is chosen since a resolution must be picked even if no
// constraints are provided. We don't just select the maximum supported
// resolution since higher resolution cost more in terms of complexity and
// many cameras perform worse at its maximum supported resolution.
const media::VideoCaptureFormat& GetBestCaptureFormat(
    const media::VideoCaptureFormats& formats) {
  DCHECK(!formats.empty());

  int default_area =
      MediaStreamVideoSource::kDefaultWidth *
      MediaStreamVideoSource::kDefaultHeight;

  media::VideoCaptureFormats::const_iterator it = formats.begin();
  media::VideoCaptureFormats::const_iterator best_it = formats.begin();
  int best_diff = std::numeric_limits<int>::max();
  for (; it != formats.end(); ++it) {
    int diff = abs(default_area -
                   it->frame_size.width() * it->frame_size.height());
    if (diff < best_diff) {
      best_diff = diff;
      best_it = it;
    }
  }
  return *best_it;
}

}  // anonymous namespace

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
    const blink::WebMediaStreamTrack& track,
    const blink::WebMediaConstraints& constraints,
    const ConstraintsCallback& callback) {
  DCHECK(CalledOnValidThread());
  requested_constraints_.push_back(RequestedConstraints(constraints,
                                                        callback));
  switch (state_) {
    case NEW: {
      // Tab capture and Screen capture needs the maximum requested height
      // and width to decide on the resolution.
      blink::WebString max_width;
      int max_requested_width = 0;
      if (constraints.getMandatoryConstraintValue(kMaxWidth, max_width))
        base::StringToInt(max_width.utf8(), &max_requested_width);

      int max_requested_height = 0;
      blink::WebString max_height;
      if (constraints.getMandatoryConstraintValue(kMaxHeight, max_height))
        base::StringToInt(max_height.utf8(), &max_requested_height);

      state_ = RETRIEVING_CAPABILITIES;
      GetCurrentSupportedFormats(max_requested_width,
                                 max_requested_height);

      break;
    }
    case STARTING:
    case RETRIEVING_CAPABILITIES: {
      // The |callback| will be triggered once the delegate has started or
      // the capabilitites has been retrieved.
      break;
    }
    case ENDED:
    case STARTED: {
      // Currently, reconfiguring the source is not supported.
      FinalizeAddTrack();
    }
  }
}

void MediaStreamVideoSource::RemoveTrack(
    const blink::WebMediaStreamTrack& track) {
  // TODO(ronghuawu): What should be done here? Do we really need RemoveTrack?
}

void MediaStreamVideoSource::InitAdapter() {
  if (adapter_)
    return;
  // Create the webrtc::MediaStreamVideoSourceInterface adapter.
  // It needs the constraints so that constraints used by a PeerConnection
  // will be available such as constraints for CPU adaptation and a tab
  // capture.
  bool is_screeencast =
      device_info().device.type == MEDIA_TAB_VIDEO_CAPTURE ||
      device_info().device.type == MEDIA_DESKTOP_VIDEO_CAPTURE;
  capture_adapter_ = factory_->CreateVideoCapturer(is_screeencast);
  capture_adapter_->SetRequestedFormat(current_format_);
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
}

void MediaStreamVideoSource::DeliverVideoFrame(
    const scoped_refptr<media::VideoFrame>& frame) {
  if (capture_adapter_)
    capture_adapter_->OnFrameCaptured(frame);
}

void MediaStreamVideoSource::OnSupportedFormats(
    const media::VideoCaptureFormats& formats) {
  DCHECK(CalledOnValidThread());
  DCHECK_EQ(RETRIEVING_CAPABILITIES, state_);

  supported_formats_ = formats;
  if (!FindBestFormatWithConstraints(supported_formats_, &current_format_,
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
    blink::WebMediaConstraints* resulting_constraints) {
  // Find the first constraints that we can fulfilled.
  for (std::vector<RequestedConstraints>::iterator request_it =
           requested_constraints_.begin();
       request_it != requested_constraints_.end(); ++request_it) {
    const blink::WebMediaConstraints& requested_constraints =
        request_it->constraints;

    media::VideoCaptureFormats filtered_formats =
        FilterFormats(requested_constraints, formats);
    if (filtered_formats.size() > 0) {
      // A request with constraints that can be fulfilled.
      *best_format = GetBestCaptureFormat(filtered_formats);
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
  // TODO(perkj): Notify all registered tracks.
}

MediaStreamVideoSource::RequestedConstraints::RequestedConstraints(
    const blink::WebMediaConstraints& constraints,
    const ConstraintsCallback& callback)
    : constraints(constraints), callback(callback) {
}

MediaStreamVideoSource::RequestedConstraints::~RequestedConstraints() {
}

}  // namespace content
