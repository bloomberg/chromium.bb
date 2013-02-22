// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_utils.h"

#include "chrome/browser/media/audio_stream_indicator.h"
#include "chrome/browser/media/media_capture_devices_dispatcher.h"
#include "chrome/browser/media/media_stream_capture_indicator.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace chrome {

bool ShouldShowProjectingIndicator(content::WebContents* contents) {
  int render_process_id = contents->GetRenderProcessHost()->GetID();
  int render_view_id = contents->GetRenderViewHost()->GetRoutingID();
  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()->
          GetMediaStreamCaptureIndicator();
  return indicator->IsBeingMirrored(render_process_id, render_view_id);
}

bool ShouldShowRecordingIndicator(content::WebContents* contents) {
  int render_process_id = contents->GetRenderProcessHost()->GetID();
  int render_view_id = contents->GetRenderViewHost()->GetRoutingID();
  scoped_refptr<MediaStreamCaptureIndicator> indicator =
      MediaCaptureDevicesDispatcher::GetInstance()->
          GetMediaStreamCaptureIndicator();
  // The projecting indicator takes precedence over the recording indicator, but
  // if we are projecting and we don't handle the projecting case we want to
  // still show the recording indicator.
  return indicator->IsCapturingUserMedia(render_process_id, render_view_id) ||
         indicator->IsBeingMirrored(render_process_id, render_view_id);
}

bool IsPlayingAudio(content::WebContents* contents) {
  AudioStreamIndicator* audio_indicator =
      MediaCaptureDevicesDispatcher::GetInstance()->GetAudioStreamIndicator();
  return audio_indicator->IsPlayingAudio(contents);
}

}  // namespace chrome
