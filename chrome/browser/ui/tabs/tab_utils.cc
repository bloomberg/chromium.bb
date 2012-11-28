// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ui/tabs/tab_utils.h"

#include "chrome/browser/media/media_internals.h"
#include "chrome/browser/media/media_stream_capture_indicator.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/browser/render_view_host.h"
#include "content/public/browser/web_contents.h"

namespace chrome {

bool ShouldShowProjectingIndicator(content::WebContents* contents) {
  int render_process_id = contents->GetRenderProcessHost()->GetID();
  int render_view_id = contents->GetRenderViewHost()->GetRoutingID();
  scoped_refptr<MediaStreamCaptureIndicator> capture_indicator =
      MediaInternals::GetInstance()->GetMediaStreamCaptureIndicator();
  return capture_indicator->IsProcessCapturingTab(render_process_id,
                                                  render_view_id);
}

bool ShouldShowRecordingIndicator(content::WebContents* contents) {
  int render_process_id = contents->GetRenderProcessHost()->GetID();
  int render_view_id = contents->GetRenderViewHost()->GetRoutingID();
  scoped_refptr<MediaStreamCaptureIndicator> capture_indicator =
      MediaInternals::GetInstance()->GetMediaStreamCaptureIndicator();
  // The projecting indicator takes precedence over the recording indicator, but
  // if we are projecting and we don't handle the projecting case we want to
  // still show the recording indicator.
  return capture_indicator->IsProcessCapturing(render_process_id,
                                               render_view_id) ||
         capture_indicator->IsProcessCapturingTab(render_process_id,
                                                  render_view_id);
}

}  // namespace chrome
