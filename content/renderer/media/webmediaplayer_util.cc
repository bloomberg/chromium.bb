// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/renderer/media/webmediaplayer_util.h"

#include <math.h>

#include "media/base/media_keys.h"
#include "third_party/WebKit/public/web/WebMediaPlayerClient.h"

namespace content {

// Compile asserts shared by all platforms.

#define COMPILE_ASSERT_MATCHING_ENUM(name) \
  COMPILE_ASSERT( \
  static_cast<int>(WebKit::WebMediaPlayerClient::MediaKeyErrorCode ## name) == \
  static_cast<int>(media::MediaKeys::k ## name ## Error), \
  mismatching_enums)
COMPILE_ASSERT_MATCHING_ENUM(Unknown);
COMPILE_ASSERT_MATCHING_ENUM(Client);
#undef COMPILE_ASSERT_MATCHING_ENUM

base::TimeDelta ConvertSecondsToTimestamp(double seconds) {
  double microseconds = seconds * base::Time::kMicrosecondsPerSecond;
  return base::TimeDelta::FromMicroseconds(
      microseconds > 0 ? microseconds + 0.5 : ceil(microseconds - 0.5));
}

WebKit::WebTimeRanges ConvertToWebTimeRanges(
    const media::Ranges<base::TimeDelta>& ranges) {
  WebKit::WebTimeRanges result(ranges.size());
  for (size_t i = 0; i < ranges.size(); i++) {
    result[i].start = ranges.start(i).InSecondsF();
    result[i].end = ranges.end(i).InSecondsF();
  }
  return result;
}

WebKit::WebMediaPlayer::NetworkState PipelineErrorToNetworkState(
    media::PipelineStatus error) {
  DCHECK_NE(error, media::PIPELINE_OK);

  switch (error) {
    case media::PIPELINE_ERROR_NETWORK:
    case media::PIPELINE_ERROR_READ:
      return WebKit::WebMediaPlayer::NetworkStateNetworkError;

    // TODO(vrk): Because OnPipelineInitialize() directly reports the
    // NetworkStateFormatError instead of calling OnPipelineError(), I believe
    // this block can be deleted. Should look into it! (crbug.com/126070)
    case media::PIPELINE_ERROR_INITIALIZATION_FAILED:
    case media::PIPELINE_ERROR_COULD_NOT_RENDER:
    case media::PIPELINE_ERROR_URL_NOT_FOUND:
    case media::DEMUXER_ERROR_COULD_NOT_OPEN:
    case media::DEMUXER_ERROR_COULD_NOT_PARSE:
    case media::DEMUXER_ERROR_NO_SUPPORTED_STREAMS:
    case media::DECODER_ERROR_NOT_SUPPORTED:
      return WebKit::WebMediaPlayer::NetworkStateFormatError;

    case media::PIPELINE_ERROR_DECODE:
    case media::PIPELINE_ERROR_ABORT:
    case media::PIPELINE_ERROR_OPERATION_PENDING:
    case media::PIPELINE_ERROR_INVALID_STATE:
      return WebKit::WebMediaPlayer::NetworkStateDecodeError;

    case media::PIPELINE_ERROR_DECRYPT:
      // TODO(xhwang): Change to use NetworkStateDecryptError once it's added in
      // Webkit (see http://crbug.com/124486).
      return WebKit::WebMediaPlayer::NetworkStateDecodeError;

    case media::PIPELINE_OK:
    case media::PIPELINE_STATUS_MAX:
      NOTREACHED() << "Unexpected status! " << error;
  }
  return WebKit::WebMediaPlayer::NetworkStateFormatError;
}

}  // namespace content
