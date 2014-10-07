// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "athena/content/media_utils.h"

#include "athena/activity/public/activity.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/media_stream_capture_indicator.h"
#include "content/browser/media/audio_stream_monitor.h"
#include "content/public/browser/web_contents.h"

namespace athena {

Activity::ActivityMediaState GetActivityMediaState(
    content::WebContents* contents) {
  // TODO(skuhne): This needs to be refactored and / or moved out of Chrome.
  // See crbug.com/420677.
  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()->
          GetMediaStreamCaptureIndicator();
  if (indicator.get() && indicator->IsCapturingUserMedia(contents))
    return Activity::ACTIVITY_MEDIA_STATE_RECORDING;

  if (contents->GetCapturerCount())
    return Activity::ACTIVITY_MEDIA_STATE_CAPTURING;

  DCHECK(content::AudioStreamMonitor::monitoring_available());
  return contents->WasRecentlyAudible() ?
      Activity::ACTIVITY_MEDIA_STATE_AUDIO_PLAYING :
      Activity::ACTIVITY_MEDIA_STATE_NONE;
}

}  // namespace athena
