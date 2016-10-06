// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/media/android/browser_media_session_manager.h"

#include "base/optional.h"
#include "content/browser/media/session/media_session.h"
#include "content/browser/web_contents/web_contents_impl.h"
#include "content/common/media/media_metadata_sanitizer.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/render_process_host.h"
#include "content/public/common/media_metadata.h"

namespace content {

BrowserMediaSessionManager::BrowserMediaSessionManager(
    RenderFrameHost* render_frame_host)
    : render_frame_host_(render_frame_host) {}

void BrowserMediaSessionManager::OnSetMetadata(
    int session_id,
    const base::Optional<MediaMetadata>& insecure_metadata) {
  // When receiving a MediaMetadata, the browser process can't trust that it is
  // coming from a known and secure source. It must be processed accordingly.
  if (insecure_metadata.has_value() &&
      !MediaMetadataSanitizer::CheckSanity(insecure_metadata.value())) {
    render_frame_host_->GetProcess()->ShutdownForBadMessage(
        RenderProcessHost::CrashReportMode::GENERATE_CRASH_DUMP);
    return;
  }

  NOTIMPLEMENTED();
}

}  // namespace content
